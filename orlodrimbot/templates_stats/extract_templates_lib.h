#ifndef EXTRACT_TEMPLATES_LIB_H
#define EXTRACT_TEMPLATES_LIB_H

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>
#include "mwclient/parser.h"
#include "mwclient/wiki.h"

class TemplateExtractor {
public:
  explicit TemplateExtractor(mwc::Wiki* wiki);
  void readTemplates(const std::string& fileName);
  void readRedirects(const std::string& fileName);
  void processDump(const std::string& outputFileName);

private:
  bool extractInvokedModule(const std::string& str, std::string& module);
  void extractFromParsedCode(const std::string& title, const wikicode::List& parsedCode,
                             std::vector<std::string>* list);
  void processPage(const std::string& title, const std::string& wcode);

  mwc::Wiki* m_wiki;
  mwc::SiteInfo m_siteInfo;
  std::unordered_map<std::string, std::string> m_templatesAndRedirects;
  FILE* m_outputFile = nullptr;
};

#endif
