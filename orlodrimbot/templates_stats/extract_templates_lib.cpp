#include "extract_templates_lib.h"
#include <re2/re2.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/parser.h"
#include "mwclient/util/include_tags.h"
#include "mwclient/util/xml_dump.h"
#include "mwclient/wiki.h"

using std::string;
using std::unordered_map;
using std::vector;

TemplateExtractor::TemplateExtractor(mwc::Wiki* wiki) : m_wiki(wiki) {
  m_siteInfo = m_wiki->siteInfo();
}

void TemplateExtractor::readTemplates(const string& fileName) {
  std::ifstream is(fileName.c_str());
  CBL_ASSERT(is) << "Cannot open '" << fileName << "'";
  string line;
  while (getline(is, line)) {
    m_templatesAndRedirects[line];
  }
}

void TemplateExtractor::readRedirects(const string& fileName) {
  CBL_ASSERT(!m_templatesAndRedirects.empty());
  std::ifstream is(fileName.c_str());
  CBL_ASSERT(is) << "Cannot open '" << fileName << "'";
  const string templatePrefix = m_siteInfo.namespaces().at(mwc::NS_TEMPLATE).name + ":";
  string line, title, target;
  while (getline(is, line)) {
    int i = line.find('|');
    CBL_ASSERT(i != -1);
    title = line.substr(0, i);
    target = line.substr(i + 1);
    CBL_ASSERT(title.starts_with(templatePrefix));
    if (!target.starts_with(templatePrefix)) continue;
    target = target.substr(templatePrefix.size());
    CBL_ASSERT(!target.empty());
    unordered_map<string, string>::iterator it = m_templatesAndRedirects.find(target);
    if (it != m_templatesAndRedirects.end() && it->second.empty()) {
      title = title.substr(templatePrefix.size());
      m_templatesAndRedirects[title] = target;
    }
  }
}

void TemplateExtractor::processDump(const string& outputFileName) {
  if (outputFileName.empty()) {
    m_outputFile = stdout;
  } else {
    m_outputFile = fopen(outputFileName.c_str(), "w");
    CBL_ASSERT(m_outputFile) << "Cannot open '" << outputFileName << "'";
  }

  mwc::PagesDump dump;
  string wcode;
  for (int iPage = 1; dump.getArticle(); iPage++) {
    if (iPage % 10000 == 0) {
      CBL_INFO << iPage << " pages lues";
    }
    if (dump.title().starts_with("Module:") && !dump.title().ends_with("/Documentation")) {
      // Do not try to find template inclusions in modules as they are often built dynamically by concatenating strings.
      // For instance, '{{' .. variable .. '}}' is not an inclusion of a template named "' .. variable .. '".
      // Documentation pages of modules contain normal wikicode so they are not skipped.
      continue;
    }
    dump.getContent(wcode);
    processPage(dump.title(), wcode);
  }

  if (!outputFileName.empty()) {
    const int fcloseResult = fclose(m_outputFile);
    CBL_ASSERT_EQ(fcloseResult, 0) << "Cannot close '" << outputFileName << "'";
  }
}

bool TemplateExtractor::extractInvokedModule(const string& str, string& module) {
  static const re2::RE2 reModuleInvoke("(?i:#invoke|#invoque)\\s*:(.*)");
  string moduleShortName;
  if (!RE2::FullMatch(str, reModuleInvoke, &moduleShortName)) {
    return false;
  }
  module = m_wiki->normalizeTitle("Module:" + moduleShortName);
  return true;
}

void TemplateExtractor::extractFromParsedCode(const string& title, const wikicode::List& parsedCode,
                                              vector<string>* list) {
  string templateName;
  for (const wikicode::Template& template_ : parsedCode.getTemplates()) {
    if (extractInvokedModule(template_.name(), templateName)) {
      // Module
    } else {
      // Template?
      //  {{MyTemplate}} => yes
      //  {{Template:MyTemplate}} => yes, means the same thing
      //  {{:Some article}}, {{User:Some user page}} => no, this is something from another namespace.
      mwc::TitleParts titleParts = m_wiki->parseTitle(template_.name(), mwc::NS_TEMPLATE);
      if (titleParts.namespaceNumber != mwc::NS_TEMPLATE) continue;
      templateName = string(titleParts.unprefixedTitle());
    }
    unordered_map<string, string>::const_iterator it = m_templatesAndRedirects.find(templateName);
    if (it == m_templatesAndRedirects.end()) continue;
    const string& normalizedTemplateName = it->second.empty() ? it->first : it->second;
    string bufferNorm = cbl::collapseSpace(template_.toString());
    if (list != nullptr) {
      list->push_back(string());
      cbl::append(list->back(), normalizedTemplateName, "|", title, "|", bufferNorm, "\n");
    } else {
      int fprintfResult =
          fprintf(m_outputFile, "%s|%s|%s\n", normalizedTemplateName.c_str(), title.c_str(), bufferNorm.c_str());
      CBL_ASSERT(fprintfResult >= 0);
    }
  }
}

void TemplateExtractor::processPage(const string& title, const string& wcode) {
  static const re2::RE2 reIncludeTag("</?(?i:includeonly|noinclude|onlyinclude)");
  wikicode::List parsedCode;
  if (!RE2::PartialMatch(wcode, reIncludeTag)) {
    parsedCode = wikicode::parse(wcode);
    extractFromParsedCode(title, parsedCode, nullptr);
  } else {
    static string notTranscluded, transcluded;
    mwc::include_tags::parse(wcode, &notTranscluded, &transcluded);

    static vector<string> listNotTranscluded;
    listNotTranscluded.clear();
    parsedCode = wikicode::parse(notTranscluded);
    extractFromParsedCode(title, parsedCode, &listNotTranscluded);
    std::sort(listNotTranscluded.begin(), listNotTranscluded.end());

    static vector<string> listTranscluded;
    listTranscluded.clear();
    parsedCode = wikicode::parse(transcluded);
    extractFromParsedCode(title, parsedCode, &listTranscluded);
    std::sort(listTranscluded.begin(), listTranscluded.end());

    static vector<string> fullList;
    fullList.resize(listTranscluded.size() + listNotTranscluded.size());
    int numTemplates = set_union(listNotTranscluded.begin(), listNotTranscluded.end(), listTranscluded.begin(),
                                 listTranscluded.end(), fullList.begin()) -
                       fullList.begin();

    for (int i = 0; i < numTemplates; i++) {
      int fwriteResult = fwrite(fullList[i].c_str(), 1, fullList[i].size(), m_outputFile);
      CBL_ASSERT(fwriteResult >= 0);
    }
  }
}
