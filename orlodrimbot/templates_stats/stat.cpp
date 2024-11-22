#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "cbl/args_parser.h"
#include "cbl/directory.h"
#include "cbl/file.h"
#include "cbl/html_entities.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "cbl/unicode_fr.h"
#include "mwclient/parser.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/wiki.h"
#include "side_template_data.h"
#include "templateinfo.h"

using cbl::encodeURIComponent;
using mwc::NS_MEDIAWIKI;
using mwc::NS_USER;
using mwc::Wiki;
using std::ifstream;
using std::make_unique;
using std::map;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

using MiniDump = map<string, string>;

class IndexLine {
public:
  IndexLine() : numPages(0), numErrors(0) {}
  int numPages;
  int numErrors;
};

class InclusionsReader {
public:
  virtual bool read(const string& templateName, string& page, string& code) = 0;
  virtual ~InclusionsReader() = default;
};

class UncompressedInclusionsReader : public InclusionsReader {
public:
  explicit UncompressedInclusionsReader(const string& fileName) {
    m_file.open(fileName.c_str());
    CBL_ASSERT(m_file) << "Cannot open '" << fileName << "'";
    readNextLine();
  }
  bool read(const string& templateName, string& page, string& code) override {
    if (m_currentTemplate != templateName) {
      CBL_ASSERT(cbl::concat(m_currentTemplate, "|") > templateName + "|" || m_currentTemplate.empty());
      return false;
    }
    size_t pageStart = m_currentTemplate.size() + 1;
    size_t pageEnd = m_currentLine.find('|', pageStart);
    CBL_ASSERT(pageEnd != string::npos);
    page.assign(m_currentLine, pageStart, pageEnd - pageStart);
    code.assign(m_currentLine, pageEnd + 1);
    readNextLine();
    return true;
  }

private:
  void readNextLine() {
    if (!getline(m_file, m_currentLine)) {
      m_currentLine.clear();
      m_currentTemplate = "";
    } else {
      size_t pipePosition = m_currentLine.find('|');
      CBL_ASSERT(pipePosition != string::npos);
      m_currentTemplate = string_view(m_currentLine).substr(0, pipePosition);
    }
  }

  ifstream m_file;
  string m_currentLine;
  string_view m_currentTemplate;
};

void processExtraction(const string& templatesCodeFileName, const string& inclusionsFileName, int outputFormats,
                       const string& textOutputFileName, const string& jsonOutputDir, const string& listByCountFileName,
                       bool noTalk, bool noUser, const string& dumpDate, int limit, Wiki& wiki,
                       const SideTemplateData& sideTemplateData) {
  std::ifstream templatesCodeStream(templatesCodeFileName.c_str());
  CBL_ASSERT(templatesCodeStream) << "Cannot open '" << templatesCodeFileName << "'";

  unique_ptr<InclusionsReader> inclusionsReader;
  inclusionsReader = make_unique<UncompressedInclusionsReader>(inclusionsFileName);

  FILE* textOutputFile = nullptr;
  if (outputFormats & OF_TEXT) {
    CBL_ASSERT(!textOutputFileName.empty());
    textOutputFile = fopen(textOutputFileName.c_str(), "w");
    CBL_ASSERT(textOutputFile != nullptr);
  }

  FILE* jsonFile = nullptr;
  if (outputFormats & OF_JSON) {
    CBL_ASSERT(!jsonOutputDir.empty());
    cbl::makeDir(jsonOutputDir);
    const string jsonFileName = jsonOutputDir + "json.dat";
    jsonFile = fopen(jsonFileName.c_str(), "w");
  }

  unordered_map<string, IndexLine> indexLines;

  FILE* listByCountFile = nullptr;
  if (!listByCountFileName.empty()) {
    listByCountFile = fopen(listByCountFileName.c_str(), "w");
    CBL_ASSERT(listByCountFile != nullptr);
  }

  int lineIndex = 0;
  string line;
  string templateName, templateCode;
  string inclusionPage, inclusionCode;
  for (int templateCount = 0; getline(templatesCodeStream, line); templateCount++) {
    if (limit != 0 && templateCount >= limit) {
      break;
    }
    {
      size_t pipePosition = line.find('|');
      CBL_ASSERT(pipePosition != string::npos);
      templateName.assign(line, 0, pipePosition);
      templateCode.assign(line, pipePosition + 1, line.size() - (pipePosition + 1));
    }
    TemplateInfo templateInfo(templateName, templateCode, sideTemplateData);
    while (inclusionsReader->read(templateName, inclusionPage, inclusionCode)) {
      lineIndex++;
      if (lineIndex % 100000 == 0) {
        std::cerr << lineIndex << " lignes lues" << std::endl;
      }
      int namespace_ = wiki.getTitleNamespace(inclusionPage);
      if (noUser && namespace_ == NS_USER) {
        continue;
      } else if (noTalk && mwc::isTalkNamespace(namespace_)) {
        continue;
      } else if ((namespace_ == NS_USER || namespace_ == NS_MEDIAWIKI) &&
                 (inclusionPage.ends_with(".js") || inclusionPage.ends_with(".css"))) {
        continue;
      }
      wikicode::List parsedCode = wikicode::parse(inclusionCode);
      if (parsedCode.size() != 1 || parsedCode[0].type() != wikicode::NT_TEMPLATE) {
        // I think I checked a long time ago that there were some special cases when a syntax error can cause
        // this. Error log disabled because those are very long lines.
        // std::cerr << "Invalid line: " << line << std::endl;
      } else {
        templateInfo.readInclusion(wiki, inclusionPage, parsedCode[0].asTemplate());
      }
    }
    if (templateInfo.namespace_() == TemplateInfo::TN_TEMPLATE) {
      if (!templateInfo.hasParameters() && templateName.ends_with("/Documentation")) {
        continue;
      }
    } else if (templateInfo.namespace_() == TemplateInfo::TN_MODULE) {
      // HACK (à mettre en liste noire avant)
      if (templateName.ends_with("/Documentation")) {
        continue;
      }
    }

    if (outputFormats & OF_TEXT) {
      templateInfo.generateInfo(textOutputFile, OF_TEXT);
      fprintf(textOutputFile, "\n");
    }
    if (outputFormats & OF_JSON) {
      // Data
      templateInfo.generateInfo(jsonFile, OF_JSON);
      fprintf(jsonFile, "\n");
    }

    if (listByCountFile != nullptr) {
      fprintf(listByCountFile, "%s|%i\n", templateName.c_str(), templateInfo.getArticlesCount());
    }
  }

  if (listByCountFile != nullptr) {
    fclose(listByCountFile);
  }

  if (outputFormats & OF_JSON) {
    fclose(jsonFile);
  }

  if (outputFormats & OF_TEXT) {
    fclose(textOutputFile);
  }
}

int parseOutputFormats(const string& outputFormatsStr) {
  int outputFormats = 0;
  for (string_view format : cbl::split(outputFormatsStr, ',')) {
    if (format == "text") {
      outputFormats |= OF_TEXT;
    } else if (format == "json") {
      outputFormats |= OF_JSON;
    } else {
      CBL_FATAL << "Invalid output format '" << format << "'";
    }
  }
  return outputFormats;
}

int main(int argc, char** argv) {
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
  string templatesCode;     // File with the code of templates (required).
  string inclusions;        // File with template inclusions (required).
  string outputFormatsStr;  // Output format (comma-separated list, possible values are 'text' and 'json').
  string textOutput;
  string jsonOutputDir;
  string listByCount;
  bool noTalk = false;
  bool noUser = false;
  string dumpDate;
  int limit = 0;
  string luaDB;
  cbl::parseArgs(argc, argv, &wikiFlags, "--templates,required", &templatesCode, "--inclusions,required", &inclusions,
                 "--format", &outputFormatsStr, "--textoutput", &textOutput, "--jsonoutput", &jsonOutputDir,
                 "--list-by-count", &listByCount, "--notalk", &noTalk, "--nouser", &noUser, "--dumpdate", &dumpDate,
                 "--limit", &limit, "--luadb,required", &luaDB);
  int outputFormats = outputFormatsStr.empty() ? OF_TEXT : parseOutputFormats(outputFormatsStr);
  if ((outputFormats & OF_TEXT) && textOutput.empty()) {
    CBL_FATAL << "Missing parameter --textoutput <file>.";
  } else if ((outputFormats & OF_JSON) && jsonOutputDir.empty()) {
    CBL_FATAL << "Missing parameter --jsonoutput <dir>.";
  }
  if (!jsonOutputDir.empty() && !jsonOutputDir.ends_with("/")) {
    jsonOutputDir += '/';
  }

  Wiki wiki;
  mwc::initWikiFromFlags(wikiFlags, wiki);
  SideTemplateData sideTemplateData;
  sideTemplateData.loadFromFile(luaDB);

  processExtraction(templatesCode, inclusions, outputFormats, textOutput, jsonOutputDir, listByCount, noTalk, noUser,
                    dumpDate, limit, wiki, sideTemplateData);
  return 0;
}
