#include "template_expansion_cache.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/sqlite.h"
#include "cbl/string.h"
#include "mwclient/request.h"
#include "mwclient/wiki.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::Revision;
using mwc::Wiki;
using sqlite::Database;
using sqlite::Statement;
using std::pair;
using std::string;
using std::string_view;
using std::vector;

// Computes the templates used when parsing `code`, assuming it comes from `sourcePage` at revision `sourceRevid`.
static vector<string> getTemplates(Wiki& wiki, const string& code, const string& sourcePage, mwc::revid_t sourceRevid) {
  mwc::WikiRequest request("parse");
  request.setMethod(mwc::WikiRequest::METHOD_POST_NO_SIDE_EFFECT);
  request.setParam("title", sourcePage);
  request.setParam("text", code);
  request.setRevidParam("revid", sourceRevid);
  request.setParam("prop", "templates");
  json::Value answer = request.run(wiki);

  vector<string> templates;
  for (const json::Value& value : answer["parse"]["templates"].array()) {
    templates.push_back(value["*"].str());
  }
  return templates;
}

// Finds the most recent change done on any page in `pages`.
static pair<string, Date> getMostRecentChange(Wiki& wiki, const vector<string>& pages) {
  vector<Revision> revisions;
  revisions.reserve(pages.size());
  for (const string& page : pages) {
    revisions.emplace_back().title = page;
  }
  wiki.readPages(mwc::RP_TIMESTAMP, revisions);
  Date mostRecentChange;
  string affectedPage;
  for (const Revision& revision : revisions) {
    if (revision.revid >= 0 && revision.timestamp > mostRecentChange) {
      mostRecentChange = revision.timestamp;
      affectedPage = revision.title;
    }
  }
  return {affectedPage, mostRecentChange};
}

TemplateExpansionCache::TemplateExpansionCache(mwc::Wiki* wiki, const string& databasePath) : m_wiki(wiki) {
  m_database = Database::open(databasePath, {sqlite::OPEN_OR_CREATE, sqlite::SYNC_OFF}, [](Database& database) {
    database.execMany(R"(
        CREATE TABLE expansion(
          source_page TEXT NOT NULL,
          source_revid INT NOT NULL,
          code TEXT NOT NULL,
          expanded_code TEXT NOT NULL,
          expansion_timestamp INT NOT NULL,
          templates TEXT,
          last_changed_template TEXT,
          last_changed_template_timestamp INT
        );
        CREATE UNIQUE INDEX expansion_index ON expansion(source_page, source_revid);
        CREATE INDEX expansion_timestamp_index ON expansion(expansion_timestamp);
    )");
  });
}

ExpansionResult TemplateExpansionCache::expand(const string& code, const string& sourcePage, mwc::revid_t sourceRevid) {
  sqlite::WriteTransaction transaction(m_database, CBL_HERE);
  if (!m_cleanupDoneOnce) {
    Date cacheExpiration = Date::now() - DateDiff::fromDays(180);
    m_database.exec(
        "DELETE FROM expansion WHERE expansion_timestamp <= ?1 AND NOT source_page LIKE 'Wikipédia:Éphéméride/%';",
        cacheExpiration.toTimeT());
    m_cleanupDoneOnce = true;
  }
  // The picture of the day should stay in the cache between its creation and its display, but anniversaries should be
  // reparsed at least once a year.
  Statement statement = m_database.prepareAndBind(
      R"(SELECT code, expanded_code, expansion_timestamp, templates, last_changed_template,
         last_changed_template_timestamp
         FROM expansion WHERE source_page = ?1 AND source_revid = ?2;)",
      sourcePage, sourceRevid);
  if (statement.step()) {
    if (statement.columnTextNotNull(0) == code) {
      int64_t timestamp = statement.columnInt64(5);
      vector<string> templates;
      for (string_view template_ : cbl::split(statement.columnTextNotNull(3), '|', true)) {
        templates.emplace_back(template_);
      }
      return {
          .code = statement.columnTextNotNull(1),
          .templates = templates,
          .lastChangedTemplate = statement.columnTextNotNull(4),
          .lastChangedTemplateTimestamp = timestamp == 0 ? Date() : Date::fromTimeT(timestamp),
          .fromCache = true,
      };
    }
    CBL_WARNING << "Ignoring cached template expansion for (\"" << sourcePage << "\", " << sourceRevid
                << ") because the code to expand is different";
  }

  string expandedCode = m_wiki->expandTemplates(code, sourcePage, sourceRevid);
  vector<string> templates = getTemplates(*m_wiki, code, sourcePage, sourceRevid);
  auto [lastChangedTemplate, lastChangedTemplateTimestamp] = getMostRecentChange(*m_wiki, templates);

  m_database.exec(
      R"(INSERT OR REPLACE INTO expansion
         (source_page, source_revid, code, expanded_code, expansion_timestamp, templates, last_changed_template,
          last_changed_template_timestamp)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);)",
      sourcePage, sourceRevid, code, expandedCode, Date::now().toTimeT(), cbl::join(templates, "|"),
      lastChangedTemplate, lastChangedTemplateTimestamp.isNull() ? 0 : lastChangedTemplateTimestamp.toTimeT());
  transaction.commit();

  return {
      .code = expandedCode,
      .templates = templates,
      .lastChangedTemplate = lastChangedTemplate,
      .lastChangedTemplateTimestamp = lastChangedTemplateTimestamp,
      .fromCache = false,
  };
}

void TemplateExpansionCache::resetCleanupFlag() {
  m_cleanupDoneOnce = false;
}
