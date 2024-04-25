#include "parse_templates_lib.h"
#include <re2/re2.h>
#include <cstdio>
#include <fstream>
#include <functional>
#include <istream>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include "cbl/error.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/parser.h"
#include "mwclient/util/include_tags.h"
#include "mwclient/wiki.h"

using cbl::endsWith;
using cbl::startsWith;
using mwc::Wiki;
using std::istream;
using std::ofstream;
using std::ostream;
using std::pair;
using std::set;
using std::string;
using std::string_view;
using std::unordered_set;

namespace {

// Calls processPage on each page of the simple dump read from `inputStream`.
// A simple dump is a text file containing a list of wiki pages and their content in the following format:
//
// title 1
//  content of page 1 indented
//  with one space
// title 2
//  content of page 2 indented
//  with one space
void processSimpleDump(istream& inputStream,
                       const std::function<void(const string& title, const string& content)>& processPage) {
  string line, title, content;
  inputStream.clear();
  inputStream.seekg(0);
  while (getline(inputStream, line)) {
    CBL_ASSERT(!line.empty());
    if (line[0] == ' ') {
      cbl::append(content, string_view(line).substr(1), "\n");
    } else {
      if (!title.empty()) {
        processPage(title, content);
      }
      title = line;
      content.clear();
    }
  }
  if (!title.empty()) {
    processPage(title, content);
  }
}

// Returns a pair (baseTitle, isDocPage) where isDocPage is true if title has the suffix of documentation page and
// baseTitle is the title with that suffix removed.
pair<string, bool> parseDocPageTitle(const string& title) {
  constexpr string_view DOC_PAGE_SUFFIX = "/Documentation";
  bool isDocPage = cbl::endsWith(title, DOC_PAGE_SUFFIX);
  return {isDocPage ? title.substr(0, title.size() - DOC_PAGE_SUFFIX.size()) : title, isDocPage};
}

bool shouldProcessTemplate(const string& title) {
  if (cbl::startsWith(title, "Modèle:Données/") &&
      (cbl::endsWith(title, "/évolution population") || cbl::endsWith(title, "/informations générales"))) {
    return false;
  }
  return true;
}

bool containsInvoke(const string& code) {
  // "#invoke" is the English keyword and is always supported. Variants may exist depending on the wiki language
  // ("#invoque" for the French Wikipedia).
  static const re2::RE2 reModuleInvoke("(?i:#invoke|#invoque)\\s*:");
  return RE2::PartialMatch(code, reModuleInvoke);
}

unordered_set<string> getTemplatesWithInvoke(istream& inputStream) {
  unordered_set<string> templates;
  processSimpleDump(inputStream, [&](const string& title, const string& content) {
    if (containsInvoke(content)) {
      templates.insert(title);
    }
  });
  return templates;
}

string generateCompactTemplateCode(const string& fullCode) {
  wikicode::List parsedCode = wikicode::parse(fullCode);
  string compactCode;
  for (const wikicode::Variable& variable : parsedCode.getVariables()) {
    compactCode += "{{{";
    variable.nameNode().addToBuffer(compactCode);
    compactCode += "}}}";
  }
  if (containsInvoke(fullCode)) {
    compactCode += "{{#invoke:A}}";
  }
  return compactCode;
}

void extractParams(Wiki& wiki, const string& title, bool isDocPage, const string& content, ostream& namesFile,
                   ostream& paramsFile) {
  string codeWhenTranscluded;
  mwc::include_tags::parse(content, nullptr, &codeWhenTranscluded);
  string compactCode = generateCompactTemplateCode(cbl::collapseSpace(codeWhenTranscluded));
  if (isDocPage && compactCode.empty()) {
    // In general, skip documentation pages. Keep documentation pages that have parameters (this is sometimes done to
    // support reusing the same doc page for multiple templates).
    return;
  }
  string unprefixedTitle = wiki.stripNamespace(title, mwc::NS_TEMPLATE);
  namesFile << unprefixedTitle << "\n";
  paramsFile << unprefixedTitle << "|" << compactCode << "\n";
}

void extractTemplateData(Wiki& wiki, const string& title, const string& content, ostream& outputStream) {
  // Does the code contain a "<templatedata>" section?
  static const re2::RE2 reTemplateDataStart(R"((<(?i:templatedata)(?:\s[^<>]*)?>))");
  static const re2::RE2 reTemplateDataEnd("(</(?i:templatedata)>)");
  re2::StringPiece tag;
  if (!RE2::PartialMatch(content, reTemplateDataStart, &tag)) return;
  string_view templateDataText = string_view(content).substr(tag.data() + tag.size() - content.c_str());
  if (!RE2::PartialMatch(templateDataText, reTemplateDataEnd, &tag)) return;
  templateDataText = templateDataText.substr(0, tag.data() - templateDataText.data());
  json::Value templateData;
  try {
    templateData = json::parse(templateDataText);
  } catch (const cbl::ParseError& error) {
    // TODO: Surface the list of templates with invalid templatedata somewhere.
    return;
  }
  const json::Value& paramsField = templateData["params"];
  if (paramsField.object().empty()) return;

  set<string> parameters;
  for (const auto& [paramName, paramSpec] : paramsField) {
    parameters.insert(cbl::trimAndCollapseSpace(paramName));
    for (const json::Value& alias : paramSpec["aliases"].array()) {
      parameters.insert(cbl::trimAndCollapseSpace(alias.str()));
    }
  }

  string unprefixedTitle = wiki.stripNamespace(title, mwc::NS_TEMPLATE);
  if (unprefixedTitle.empty()) {
    return;
  }
  outputStream << unprefixedTitle << "|{{" << unprefixedTitle;
  for (const string& param : parameters) {
    if (!param.empty() && param.find_first_of("|{}[]<>\n") == string::npos) {
      outputStream << "|" << param << "=";
    }
  }
  outputStream << "}}\n";
}

}  // namespace

void parseTemplatesFromDump(Wiki& wiki, istream& inputStream, const string& paramsPath, const string& namesPath,
                            const string& templateDataPath) {
  unordered_set<string> templatesWithInvoke = getTemplatesWithInvoke(inputStream);

  ofstream paramsFile(paramsPath.c_str());
  ofstream namesFile(namesPath.c_str());
  ofstream templateDataFile(templateDataPath.c_str());
  CBL_ASSERT(paramsFile) << "Cannot write to '" << paramsPath.c_str() << "'";
  CBL_ASSERT(namesFile) << "Cannot write to '" << namesPath.c_str() << "'";
  CBL_ASSERT(templateDataFile) << "Cannot write to '" << templateDataPath.c_str() << "'";

  processSimpleDump(inputStream, [&](const string& title, const string& content) {
    if (!shouldProcessTemplate(title)) {
      // Skip based on the title.
      return;
    } else if (wiki.readRedirect(content, nullptr, nullptr)) {
      // Skip because this is a redirect.
      return;
    }
    auto [titleBase, isDocPage] = parseDocPageTitle(title);
    extractParams(wiki, title, isDocPage, content, namesFile, paramsFile);
    if (templatesWithInvoke.count(titleBase) != 0) {
      extractTemplateData(wiki, titleBase, content, templateDataFile);
    }
  });
}
