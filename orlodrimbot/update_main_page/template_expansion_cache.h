#ifndef TEMPLATE_EXPANSION_CACHE_H
#define TEMPLATE_EXPANSION_CACHE_H

#include <string>
#include <vector>
#include "cbl/date.h"
#include "cbl/sqlite.h"
#include "mwclient/wiki.h"

struct ExpansionResult {
  std::string code;
  std::vector<std::string> templates;
  std::string lastChangedTemplate;
  cbl::Date lastChangedTemplateTimestamp;
  bool fromCache = false;
};

class TemplateExpansionCache {
public:
  TemplateExpansionCache(mwc::Wiki* wiki, const std::string& databasePath);
  ExpansionResult expand(const std::string& code, const std::string& sourcePage, mwc::revid_t sourceRevid);

private:
  mwc::Wiki* m_wiki = nullptr;
  sqlite::Database m_database;
};

#endif
