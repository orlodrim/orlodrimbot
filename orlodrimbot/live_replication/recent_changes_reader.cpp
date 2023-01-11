#include "recent_changes_reader.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/sqlite.h"
#include "mwclient/wiki_defs.h"
#include "continue_token.h"

using cbl::Date;
using mwc::LogEvent;
using mwc::LogEventType;
using mwc::RC_EDIT;
using mwc::RC_LOG;
using mwc::RC_NEW;
using mwc::RC_UNDEFINED;
using mwc::RecentChange;
using mwc::RecentChangeType;
using mwc::Revision;
using sqlite::Database;
using sqlite::ReadTransaction;
using sqlite::Statement;
using std::pair;
using std::string;
using std::string_view;
using std::unordered_set;
using std::vector;

namespace live_replication {
namespace {

constexpr string_view RC_CONTINUE_TOKEN = "rc";

constexpr pair<mwc::RevProp, const char*> RC_PROPERTIES[] = {
    {mwc::RP_TITLE, "title, new_title"},
    {mwc::RP_USER, "user"},
    {mwc::RP_TIMESTAMP, "timestamp"},
    {mwc::RP_SIZE, "size"},
    {mwc::RP_COMMENT, "comment"},
    {mwc::RP_REVID, "revid, old_revid, logid"},
};

constexpr pair<RecentChangeType, const char*> RC_TYPES[] = {
    {RC_EDIT, "edit"},
    {RC_NEW, "new"},
    {RC_LOG, "log"},
};

RecentChangeType getRecentChangeTypeFromString(string_view str) {
  if (str == "edit") {
    return RC_EDIT;
  } else if (str == "new") {
    return RC_NEW;
  } else if (str == "log") {
    return RC_LOG;
  } else {
    return RC_UNDEFINED;
  }
}

LogEventType getLogEventTypeFromString(string_view str) {
  if (str == "delete") {
    return mwc::LE_DELETE;
  } else if (str == "upload") {
    return mwc::LE_UPLOAD;
  } else if (str == "move") {
    return mwc::LE_MOVE;
  } else if (str == "import") {
    return mwc::LE_IMPORT;
  } else if (str == "protect") {
    return mwc::LE_PROTECT;
  } else {
    return mwc::LE_UNDEFINED;
  }
}

}  // namespace

RecentChangesReader::RecentChangesReader(const string& databasePath) {
  m_database = Database::open(databasePath, {sqlite::OPEN_READONLY});
}

void RecentChangesReader::enumRecentChanges(const RecentChangesOptions& options, const Callback& callback) {
  ReadTransaction transaction(m_database, CBL_HERE);

  int64_t nextId = -1;
  if (!options.start.isNull()) {
    Statement statement = m_database.prepareAndBind(
        "SELECT rcid FROM recentchanges WHERE timestamp >= ?1 ORDER BY timestamp, rcid LIMIT 1;",
        options.start.toTimeT());
    nextId = statement.step() ? statement.columnInt64(0) : 0;
  }
  if (options.continueToken && !options.continueToken->empty()) {
    nextId = std::max(nextId, parseContinueToken(*options.continueToken, RC_CONTINUE_TOKEN));
  }
  if (nextId == -1) {
    Statement statement = m_database.prepare("SELECT MAX(rcid) FROM recentchanges;");
    CBL_ASSERT(statement.step());
    nextId = statement.isColumnNull(0) ? 0 : statement.columnInt64(0) + 1;
  }

  string query = "SELECT rcid, type";
  if (options.includeLogDetails) {
    query += ", logtype, logaction, logparams";
  }
  for (const auto& [property, columnName] : RC_PROPERTIES) {
    if (options.properties & property) {
      query += ", ";
      query += columnName;
    }
  }
  query += " FROM recentchanges WHERE rcid >= ?1";
  if (options.type != 0) {
    bool typeAdded = false;
    for (const auto& [type, typeName] : RC_TYPES) {
      if (options.type & type) {
        query += typeAdded ? "\" OR type = \"" : " AND (type = \"";
        query += typeName;
        typeAdded = true;
      }
    }
    if (typeAdded) {
      query += "\")";
    }
  }
  query += " ORDER BY rcid;";
  Statement statement = m_database.prepareAndBind(query, nextId);

  RecentChange editRc;
  RecentChange newRc;
  RecentChange logEventRc;
  editRc.setType(RC_EDIT);
  newRc.setType(RC_NEW);
  logEventRc.setType(RC_LOG);
  int limit = options.limit;

  while (limit != 0 && statement.step()) {
    nextId = statement.columnInt64(0);
    RecentChangeType rcType = getRecentChangeTypeFromString(statement.columnTextNotNull(1));
    int columnIndex = 2;
    RecentChange* recentChange = nullptr;
    switch (rcType) {
      case RC_EDIT:
      case RC_NEW: {
        if (options.includeLogDetails) {
          columnIndex += 3;  // logtype, logaction, logparams
        }
        recentChange = rcType == RC_EDIT ? &editRc : &newRc;
        Revision& revision = recentChange->mutableRevision();
        if (options.properties & mwc::RP_TITLE) {
          revision.title = statement.columnTextNotNull(columnIndex++);
          columnIndex++;  // new_title is only used for log events.
        }
        if (options.properties & mwc::RP_USER) {
          revision.user = statement.columnTextNotNull(columnIndex++);
        }
        if (options.properties & mwc::RP_TIMESTAMP) {
          revision.timestamp = Date::fromTimeT(statement.columnInt64(columnIndex++));
        }
        if (options.properties & mwc::RP_SIZE) {
          revision.size = statement.columnInt64(columnIndex++);
        }
        if (options.properties & mwc::RP_COMMENT) {
          revision.comment = statement.columnTextNotNull(columnIndex++);
        }
        if (options.properties & mwc::RP_REVID) {
          revision.revid = statement.columnInt64(columnIndex++);
          recentChange->oldRevid = statement.columnInt64(columnIndex++);
          columnIndex++;  // logid
        }
        break;
      }
      case RC_LOG: {
        recentChange = &logEventRc;
        LogEvent& logEvent = recentChange->mutableLogEvent();
        json::Value params;
        if (options.includeLogDetails) {
          logEvent.setType(getLogEventTypeFromString(statement.columnTextNotNull(columnIndex++)));
          logEvent.action = statement.columnTextNotNull(columnIndex++);
          const char* paramsAsJson = statement.columnText(columnIndex++);
          if (paramsAsJson) {
            params = json::parse(paramsAsJson);
          }
        }
        if (options.properties & mwc::RP_TITLE) {
          logEvent.title = statement.columnTextNotNull(columnIndex++);
          if (!statement.isColumnNull(columnIndex)) {
            if (!options.includeLogDetails) {
              logEvent.setType(mwc::LE_MOVE);  // Property not requested, but this must be set before using moveParams.
            }
            if (logEvent.type() == mwc::LE_MOVE) {  // This should be true unless the database is inconsistent.
              logEvent.mutableMoveParams().newTitle = statement.columnTextNotNull(columnIndex);
              if (params["suppressredirect"].boolean()) {
                logEvent.mutableMoveParams().suppressRedirect = true;
              }
            }
          }
          columnIndex++;
        }
        if (options.properties & mwc::RP_USER) {
          logEvent.user = statement.columnTextNotNull(columnIndex++);
        }
        if (options.properties & mwc::RP_TIMESTAMP) {
          logEvent.timestamp = Date::fromTimeT(statement.columnInt64(columnIndex++));
        }
        if (options.properties & mwc::RP_SIZE) {
          columnIndex++;
        }
        if (options.properties & mwc::RP_COMMENT) {
          logEvent.comment = statement.columnTextNotNull(columnIndex++);
        }
        if (options.properties & mwc::RP_REVID) {
          columnIndex += 2;  // revid, old_revid
          logEvent.logid = statement.columnInt64(columnIndex++);
        }
        break;
      }
      default:
        continue;
    }
    if (!options.end.isNull() && recentChange->timestamp() > options.end) {
      break;
    }
    callback(*recentChange);
    if (limit != mwc::PAGER_ALL) {
      limit--;
    }
    nextId++;
  }

  if (options.continueToken) {
    *options.continueToken = buildContinueToken(RC_CONTINUE_TOKEN, nextId);
  }
}

unordered_set<string> RecentChangesReader::getRecentlyUpdatedPages(const RecentlyUpdatedPagesOptions& options) {
  unordered_set<string> titles;
  RecentChangesOptions rcOptions;
  rcOptions.properties = mwc::RP_TITLE;
  if (!options.excludedUser.empty()) {
    rcOptions.properties |= mwc::RP_USER;
  }
  rcOptions.start = options.start;
  rcOptions.end = options.end;
  rcOptions.continueToken = options.continueToken;
  enumRecentChanges(rcOptions, [&](const RecentChange& rc) {
    if (!options.excludedUser.empty() && rc.user() == options.excludedUser) {
      return;
    }
    const string& title = rc.title();
    if (!title.empty()) {
      titles.insert(title);
    }
    if (rc.type() == RC_LOG) {
      const string& newTitle = rc.logEvent().moveParams().newTitle;
      if (!newTitle.empty()) {
        titles.insert(newTitle);
      }
    }
  });
  return titles;
}

vector<LogEvent> RecentChangesReader::getRecentLogEvents(const RecentLogEventsOptions& options) {
  vector<LogEvent> logEvents;
  RecentChangesOptions rcOptions;
  rcOptions.type = RC_LOG;
  rcOptions.includeLogDetails = true;
  rcOptions.start = options.start;
  rcOptions.end = options.end;
  rcOptions.continueToken = options.continueToken;
  enumRecentChanges(rcOptions, [&](const RecentChange& rc) {
    CBL_ASSERT_EQ(rc.type(), RC_LOG);
    const LogEvent& logEvent = rc.logEvent();
    if (options.logType == mwc::LE_UNDEFINED || logEvent.type() == options.logType) {
      logEvents.push_back(logEvent);
    }
  });
  return logEvents;
}

void EmptyRecentChangesReader::enumRecentChanges(const RecentChangesOptions&, const Callback&) {}

}  // namespace live_replication
