#ifndef MOCK_WIKI_WITH_PARSE_H
#define MOCK_WIKI_WITH_PARSE_H

#include <string>
#include "cbl/json.h"
#include "mwclient/mock_wiki.h"
#include "mwclient/wiki.h"

// Version of MockWiki that supports operations used by TemplateExpansionCache.
class MockWikiWithParse : public mwc::MockWiki {
public:
  void resetDatabase() override;
  // Supports action=parse&prop=templates. Returns the templates found directly in the code (no recursive expansion).
  json::Value apiRequest(const std::string& request, const std::string& data, bool canRetry) override;
  // Performs dummy expansion of templates by adding "expanded:" before the template name.
  // Also replaces {{PAGENAME}} and {{REVISIONID}}.
  std::string expandTemplates(const std::string& code, const std::string& title, mwc::revid_t revid) override;

  // Counter incremented each time expandTemplates is called.
  int expandTemplatesCallCount = 0;
};

#endif
