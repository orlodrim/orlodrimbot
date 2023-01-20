#include "recent_changes_sync.h"
#include <cstdint>
#include <limits>
#include <string>
#include <vector>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/sqlite.h"
#include "mwclient/wiki.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::LogEvent;
using mwc::LogEventType;
using mwc::RC_EDIT;
using mwc::RC_LOG;
using mwc::RC_NEW;
using mwc::RecentChange;
using mwc::RecentChangesParams;
using mwc::Revision;
using mwc::Wiki;
using sqlite::Database;
using sqlite::ReadTransaction;
using sqlite::Statement;
using sqlite::WriteTransaction;
using std::string;
using std::vector;

namespace live_replication {
namespace {

// Keep only that number of days in the database.
// This is counted from the most recent change already committed in order to avoid any dependency on the local clock.
constexpr int MAX_DAYS_TO_KEEP = 35;

// Changes in the recentchanges table are not inserted by increasing timestamp, so it is necessary to add some overlap
// between consecutive requests to not miss any change.
// This is independent of the problem of non-increasing rcids addressed by m_secondsToIgnore.
constexpr int64_t OVERLAP_BETWEEN_RC_REQUESTS = 60;

const char* convertLogEventTypeToStr(LogEventType type) {
  switch (type) {
    case mwc::LE_DELETE:
      return "delete";
    case mwc::LE_UPLOAD:
      return "upload";
    case mwc::LE_MOVE:
      return "move";
    case mwc::LE_IMPORT:
      return "import";
    case mwc::LE_PROTECT:
      return "protect";
    default:
      break;
  }
  return nullptr;
}

vector<RecentChange> readRecentChanges(Wiki& wiki, const Date& start) {
  RecentChangesParams params;
  if (start.isNull()) {
    CBL_INFO << "Reading the most recent change to initialize the recentchanges database";
    params.limit = 1;
  } else {
    CBL_INFO << "Reading recent changes from " << start << " to now";
    params.start = start;
    params.direction = mwc::OLDEST_FIRST;
    params.limit = mwc::PAGER_ALL;
  }
  params.prop = mwc::RP_TITLE | mwc::RP_REVID | mwc::RP_USER | mwc::RP_TIMESTAMP | mwc::RP_SIZE | mwc::RP_COMMENT;
  params.type = RC_EDIT | RC_NEW | RC_LOG;
  return wiki.getRecentChanges(params);
}

}  // namespace

RecentChangesSync::RecentChangesSync(const string& databasePath) {
  m_database = Database::open(databasePath, sqlite::OpenParams(), [](Database& db) {
    CBL_INFO << "Creating new recentchanges database";
    db.execMany(R"(
        CREATE TABLE recentchanges(
          rcid INTEGER PRIMARY KEY ASC,
          timestamp INT,
          title TEXT,
          user TEXT,
          comment TEXT,
          type TEXT,
          revid INT,
          old_revid INT,
          size INT,
          logid INT,
          logtype TEXT,
          logaction TEXT,
          new_title TEXT,
          logparams TEXT
        );
        CREATE INDEX recentchanges_timestamp_index ON recentchanges(timestamp);
        CREATE INDEX recentchanges_log_index ON recentchanges(rcid) WHERE type = "log";
    )");
  });
}

void RecentChangesSync::updateDatabaseFromWiki(Wiki& wiki) {
  Date enumStart;
  {
    ReadTransaction transaction(m_database, CBL_HERE);
    Statement statement = m_database.prepare("SELECT MAX(timestamp) FROM recentchanges;");
    CBL_ASSERT(statement.step());
    enumStart =
        statement.isColumnNull(0) ? Date() : Date::fromTimeT(statement.columnInt64(0) - OVERLAP_BETWEEN_RC_REQUESTS);
  }
  // Lower bound on the timestamp at which MediaWiki reads the database, assuming that the local clock is accurate.
  Date requestDate = Date::now();
  vector<RecentChange> recentChanges = readRecentChanges(wiki, enumStart);
  if (recentChanges.empty()) {
    return;
  }
  WriteTransaction transaction(m_database, CBL_HERE);
  writeRecentChanges(recentChanges, requestDate);
  transaction.commit();
}

void RecentChangesSync::writeRecentChanges(const vector<RecentChange>& recentChanges, const Date& requestDate) {
  int64_t oldMaxRcid = 0;
  int64_t newMaxRcid = std::numeric_limits<int64_t>::max();
  Date oldMaxTimestamp;  // For logging purposes only.
  {
    Statement statement = m_database.prepare("SELECT MAX(rcid), MAX(timestamp) FROM recentchanges;");
    CBL_ASSERT(statement.step());
    oldMaxRcid = statement.isColumnNull(0) ? -1 : statement.columnInt64(0);
    oldMaxTimestamp = statement.isColumnNull(1) ? Date() : Date::fromTimeT(statement.columnInt64(1));
  }
  if (m_secondsToIgnore > 0 && oldMaxRcid != -1) {
    // Sometimes on Wikimedia wikis (~1% of the time on frwiki), the API response is missing some changes with
    // rcids <= the highest one in the response, and they will only appear at a future time.
    // Inserting rcids out of order between updates makes it impossible for clients to read a continuous stream of
    // changes without missing changes. To minimize that risk, we ignore the last m_secondsToIgnore seconds in the
    // response.
    // However, since the order of rcid and timestamp is also slightly different, we can't simply put a condition on the
    // timestamp. Instead, we first compute the smaller rcid in the time range to ignore, and then discard all changes
    // with a higher rcid.
    Date maxTimestamp = requestDate - DateDiff::fromSeconds(m_secondsToIgnore);
    for (const RecentChange& rc : recentChanges) {
      if (rc.timestamp() > maxTimestamp) {
        newMaxRcid = std::min(newMaxRcid, rc.rcid - 1);
      }
    }
  }
  Statement statement = m_database.prepare(
      "INSERT INTO recentchanges "
      "(rcid, timestamp, title, user, comment, type, revid, old_revid, size, logid, logtype, logaction, "
      "new_title, logparams) "
      "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14);");
  Statement checkRcidStatement = m_database.prepare("SELECT COUNT(*) FROM recentchanges WHERE rcid = ?1");
  for (const RecentChange& rc : recentChanges) {
    if (rc.rcid > newMaxRcid) {
      continue;
    }
    statement.reset();
    statement.bind(1, rc.rcid);
    statement.bind(2, rc.timestamp().toTimeT());
    statement.bind(3, rc.title());
    statement.bind(4, rc.user());
    if (!rc.comment().empty()) {
      statement.bind(5, rc.comment());
    }
    switch (rc.type()) {
      case RC_EDIT:
      case RC_NEW: {
        const Revision& revision = rc.revision();
        statement.bind(6, rc.type() == RC_EDIT ? "edit" : "new");
        statement.bind(7, revision.revid);
        statement.bind(8, rc.oldRevid);
        statement.bind(9, revision.size);
        break;
      }
      case RC_LOG: {
        const LogEvent& logEvent = rc.logEvent();
        const char* typeStr = convertLogEventTypeToStr(logEvent.type());
        if (typeStr == nullptr) continue;
        if (logEvent.type() == mwc::LE_MOVE) {
          statement.bind(13, rc.logEvent().moveParams().newTitle);
        }
        statement.bind(6, "log");
        statement.bind(10, logEvent.logid);
        statement.bind(11, typeStr);
        statement.bind(12, logEvent.action);
        if (logEvent.type() == mwc::LE_MOVE && logEvent.moveParams().suppressRedirect) {
          statement.bind(14, R"({"suppressredirect":true})");
        }
        break;
      }
      default:
        continue;
    }
    if (rc.rcid <= oldMaxRcid) {
      checkRcidStatement.reset();
      checkRcidStatement.bind(1, rc.rcid);
      CBL_ASSERT(checkRcidStatement.step());
      if (checkRcidStatement.columnInt(0) == 0) {
        CBL_WARNING << "Ignoring change with rcid smaller than latest change from the previous update (rcid=" << rc.rcid
                    << ", title=" << rc.title() << ", timestamp=" << rc.timestamp() << ")";
      }
      continue;
    } else if (rc.timestamp() < oldMaxTimestamp) {
      int diff = (oldMaxTimestamp - rc.timestamp()).seconds();
      if (diff >= 10) {
        CBL_INFO << "Change inserted " << diff << " seconds before the most recent change from the previous update";
      }
    }
    try {
      statement.step();
    } catch (const sqlite::PrimaryKeyConstraintError&) {
      // We already check before that we do not re-add changes from the previous update.
      // This error can only happen if there are duplicate rcids within the same API response.
      CBL_ERROR << "Duplicate rcid in the changes returned by the API: " << rc.rcid;
    }
  }
  if (!recentChanges.empty()) {
    int64_t dropBefore = recentChanges.back().timestamp().toTimeT() - 86400 * MAX_DAYS_TO_KEEP;
    m_database.exec("DELETE FROM recentchanges WHERE timestamp < ?1;", dropBefore);
  }
}

}  // namespace live_replication
