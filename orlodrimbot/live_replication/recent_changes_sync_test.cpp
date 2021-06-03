#include "recent_changes_sync.h"
#include <time.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/sqlite.h"
#include "cbl/tempfile.h"
#include "cbl/unittest.h"
#include "mwclient/wiki.h"
#include "recent_changes_test_util.h"

using cbl::Date;
using mwc::LogEvent;
using mwc::RC_EDIT;
using mwc::RC_LOG;
using mwc::RecentChange;
using mwc::Revision;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::vector;

namespace live_replication {

string formatDate(time_t timestamp) {
  return Date::fromTimeT(timestamp).toISO8601();
}

class RecentChangesSyncTest : public cbl::Test {
private:
  void setUp() override {
    m_wiki = std::make_unique<RCSyncMockWiki>();
    m_recentChangesSync = std::make_unique<RecentChangesSync>(":memory:");
  }

  void checkRCRows(const vector<string>& expectedRows) {
    sqlite::ReadTransaction transaction(m_recentChangesSync->m_database, CBL_HERE);
    sqlite::Statement statement = m_recentChangesSync->m_database.prepare(
        "SELECT rcid, timestamp, type, title, user, logtype, logaction, new_title FROM recentchanges ORDER BY rcid;");
    int expectedRowsCount = expectedRows.size();
    int rowIndex;
    for (rowIndex = 0; statement.step(); rowIndex++) {
      CBL_ASSERT(rowIndex < expectedRowsCount)
          << "Too many rows in recentchanges; first extra row rcid is " << statement.columnInt64(0);
      string actualRow;
      actualRow += std::to_string(statement.columnInt(0));
      actualRow += '|';
      actualRow += formatDate(statement.columnInt64(1));
      for (int i = 2; i <= 7; i++) {
        actualRow += '|';
        actualRow += statement.columnTextNotNull(i);
      }
      CBL_ASSERT_EQ(actualRow, expectedRows[rowIndex]);
    }
    CBL_ASSERT_EQ(rowIndex, expectedRowsCount);
  }

  CBL_TEST_CASE(InitializeAndUpdate) {
    m_wiki->addRecentChange(makeRC(1001, "2005-01-01T11:00:00Z", "User 1", "Article 1"));
    m_wiki->addRecentChange(makeRC(1002, "2005-01-01T12:00:00Z", "User 2", "Article 2"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    checkRCRows({"1002|2005-01-01T12:00:00Z|edit|Article 2|User 2|||"});

    m_wiki->addRecentChange(makeRC(1003, "2005-01-01T12:00:00Z", "User 3", "Article 3"));
    m_wiki->addRecentChange(makeRC(1005, "2005-01-01T12:01:00Z", "User 4", "Article 4"));
    m_wiki->addRecentChange(makeRC(1006, "2005-01-01T12:01:00Z", "User 5", "Article 5"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    checkRCRows({
        "1002|2005-01-01T12:00:00Z|edit|Article 2|User 2|||",
        "1003|2005-01-01T12:00:00Z|edit|Article 3|User 3|||",
        "1005|2005-01-01T12:01:00Z|edit|Article 4|User 4|||",
        "1006|2005-01-01T12:01:00Z|edit|Article 5|User 5|||",
    });
  }

  CBL_TEST_CASE(TimestampGoingBackward) {
    m_wiki->addRecentChange(makeRC(1001, "2005-01-01T00:00:00Z", "User 1", "Article 1"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    m_wiki->addRecentChange(makeRC(1002, "2005-01-01T00:00:08Z", "User 2", "Article 2"));
    m_wiki->addRecentChange(makeRC(1003, "2005-01-01T00:00:24Z", "User 4", "Article 4"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    checkRCRows({
        "1001|2005-01-01T00:00:00Z|edit|Article 1|User 1|||",
        "1002|2005-01-01T00:00:08Z|edit|Article 2|User 2|||",
        "1003|2005-01-01T00:00:24Z|edit|Article 4|User 4|||",
    });

    m_wiki->addRecentChange(makeRC(1004, "2005-01-01T00:00:16Z", "User 3", "Article 3"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    checkRCRows({
        "1001|2005-01-01T00:00:00Z|edit|Article 1|User 1|||",
        "1002|2005-01-01T00:00:08Z|edit|Article 2|User 2|||",
        "1003|2005-01-01T00:00:24Z|edit|Article 4|User 4|||",
        "1004|2005-01-01T00:00:16Z|edit|Article 3|User 3|||",
    });
  }

  CBL_TEST_CASE(DeleteOldRows) {
    m_wiki->addRecentChange(makeRC(1001, "2005-01-01T00:00:00Z", "User 1", "Article 1"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    m_wiki->addRecentChange(makeRC(1002, "2005-01-01T00:00:05Z", "User 2", "Article 2"));
    m_wiki->addRecentChange(makeRC(1003, "2005-01-01T02:00:00Z", "User 3", "Article 3"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    checkRCRows({
        "1001|2005-01-01T00:00:00Z|edit|Article 1|User 1|||",
        "1002|2005-01-01T00:00:05Z|edit|Article 2|User 2|||",
        "1003|2005-01-01T02:00:00Z|edit|Article 3|User 3|||",
    });

    // Changes older than 35 days are removed.
    m_wiki->addRecentChange(makeRC(1004, "2005-02-05T01:00:00Z", "User 4", "Article 4"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    checkRCRows({
        "1003|2005-01-01T02:00:00Z|edit|Article 3|User 3|||",
        "1004|2005-02-05T01:00:00Z|edit|Article 4|User 4|||",
    });
  }

  CBL_TEST_CASE(AllTypesOfRC) {
    m_wiki->addRecentChange(makeRC(1001, "2005-01-01T00:01:00Z", "User 1", "Article 1"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    m_wiki->addRecentChange(makeRC(1002, "2005-01-01T00:02:00Z", "User 2", "Article 2", mwc::RC_NEW));
    m_wiki->addRecentChange(
        makeLogRC(1003, 503, mwc::LE_DELETE, "delete", "2005-01-01T00:03:00Z", "User 3", "Article 3"));
    m_wiki->addRecentChange(
        makeLogRC(1004, 504, mwc::LE_DELETE, "restore", "2005-01-01T00:04:00Z", "User 4", "Article 4"));
    m_wiki->addRecentChange(
        makeLogRC(1005, 505, mwc::LE_PROTECT, "protect", "2005-01-01T00:05:00Z", "User 5", "Article 5"));
    m_wiki->addRecentChange(
        makeLogRC(1006, 506, mwc::LE_MOVE, "move", "2005-01-01T00:06:00Z", "User 6", "Article 6", "Article 6 renamed"));
    m_wiki->addRecentChange(
        makeLogRC(1007, 507, mwc::LE_IMPORT, "interwiki", "2005-01-01T00:07:00Z", "User 7", "Article 7"));
    m_wiki->addRecentChange(
        makeLogRC(1008, 509, mwc::LE_UPLOAD, "upload", "2005-01-01T00:08:00Z", "User 8", "Article 8"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);

    checkRCRows({
        "1001|2005-01-01T00:01:00Z|edit|Article 1|User 1|||",
        "1002|2005-01-01T00:02:00Z|new|Article 2|User 2|||",
        "1003|2005-01-01T00:03:00Z|log|Article 3|User 3|delete|delete|",
        "1004|2005-01-01T00:04:00Z|log|Article 4|User 4|delete|restore|",
        "1005|2005-01-01T00:05:00Z|log|Article 5|User 5|protect|protect|",
        "1006|2005-01-01T00:06:00Z|log|Article 6|User 6|move|move|Article 6 renamed",
        "1007|2005-01-01T00:07:00Z|log|Article 7|User 7|import|interwiki|",
        "1008|2005-01-01T00:08:00Z|log|Article 8|User 8|upload|upload|",
    });
  }

  CBL_TEST_CASE(AllPropertiesOfEdit) {
    RecentChange rc;
    rc.setType(RC_EDIT);
    rc.rcid = 1001;
    rc.oldRevid = 40;
    rc.oldSize = 123;
    Revision& revision = rc.mutableRevision();
    revision.title = "Page 1";
    revision.revid = 50;
    revision.timestamp = Date::fromISO8601("2005-01-01T00:01:00Z");
    revision.user = "User 1";
    revision.size = 1234;
    revision.comment = "Comment for edit";
    m_wiki->addRecentChange(std::move(rc));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    sqlite::ReadTransaction transaction(m_recentChangesSync->m_database, CBL_HERE);
    sqlite::Statement statement = m_recentChangesSync->m_database.prepare(
        "SELECT timestamp, title, user, comment, type, revid, old_revid FROM recentchanges WHERE rcid = 1001;");
    CBL_ASSERT(statement.step());
    CBL_ASSERT_EQ(Date::fromTimeT(statement.columnInt64(0)), Date::fromISO8601("2005-01-01T00:01:00Z"));
    CBL_ASSERT_EQ(string_view(statement.columnTextNotNull(1)), "Page 1");
    CBL_ASSERT_EQ(string_view(statement.columnTextNotNull(2)), "User 1");
    CBL_ASSERT_EQ(string_view(statement.columnTextNotNull(3)), "Comment for edit");
    CBL_ASSERT_EQ(string_view(statement.columnTextNotNull(4)), "edit");
    CBL_ASSERT_EQ(statement.columnInt64(5), 50);
    CBL_ASSERT_EQ(statement.columnInt64(6), 40);
  }

  CBL_TEST_CASE(AllPropertiesOfLog) {
    RecentChange rc;
    rc.setType(RC_LOG);
    rc.rcid = 1001;
    LogEvent& logEvent = rc.mutableLogEvent();
    logEvent.setType(mwc::LE_MOVE);
    logEvent.title = "Page 1";
    logEvent.logid = 30;
    logEvent.timestamp = Date::fromISO8601("2005-01-01T00:01:00Z");
    logEvent.user = "User 1";
    logEvent.action = "move";
    logEvent.comment = "Comment for log";
    logEvent.mutableMoveParams().newTitle = "Page 2";
    m_wiki->addRecentChange(std::move(rc));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    sqlite::ReadTransaction transaction(m_recentChangesSync->m_database, CBL_HERE);
    sqlite::Statement statement = m_recentChangesSync->m_database.prepare(
        "SELECT timestamp, title, user, comment, type, logid, logtype, logaction, new_title FROM recentchanges "
        "WHERE rcid = 1001;");
    CBL_ASSERT(statement.step());
    CBL_ASSERT_EQ(Date::fromTimeT(statement.columnInt64(0)), Date::fromISO8601("2005-01-01T00:01:00Z"));
    CBL_ASSERT_EQ(string_view(statement.columnTextNotNull(1)), "Page 1");
    CBL_ASSERT_EQ(string_view(statement.columnTextNotNull(2)), "User 1");
    CBL_ASSERT_EQ(string_view(statement.columnTextNotNull(3)), "Comment for log");
    CBL_ASSERT_EQ(string_view(statement.columnTextNotNull(4)), "log");
    CBL_ASSERT_EQ(statement.columnInt64(5), 30);
    CBL_ASSERT_EQ(string_view(statement.columnTextNotNull(6)), "move");
    CBL_ASSERT_EQ(string_view(statement.columnTextNotNull(7)), "move");
    CBL_ASSERT_EQ(string_view(statement.columnTextNotNull(8)), "Page 2");
  }

  CBL_TEST_CASE(IgnoreLastSeconds) {
    m_recentChangesSync->setSecondsToIgnore(5);
    m_wiki->addRecentChange(makeRC(1001, "2005-01-01T00:00:01Z", "User 1", "Article 1"));
    Date::setFrozenValueOfNow(Date::fromISO8601("2005-01-01T00:00:01Z"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    m_wiki->addRecentChange(makeRC(1002, "2005-01-01T00:00:02Z", "User 2", "Article 2"));
    m_wiki->addRecentChange(makeRC(1003, "2005-01-01T00:00:03Z", "User 3", "Article 3"));
    m_wiki->addRecentChange(makeRC(1004, "2005-01-01T00:00:05Z", "User 5", "Article 5"));
    // First ignored change (only 4 seconds before now).
    m_wiki->addRecentChange(makeRC(1005, "2005-01-01T00:00:06Z", "User 6", "Article 6"));
    // This one would pass the timestamp test, but its rcid is higher than the previous one so we ignore it too.
    m_wiki->addRecentChange(makeRC(1006, "2005-01-01T00:00:04Z", "User 4", "Article 4"));
    m_wiki->addRecentChange(makeRC(1007, "2005-01-01T00:00:07Z", "User 7", "Article 7"));
    m_wiki->addRecentChange(makeRC(1008, "2005-01-01T00:00:08Z", "User 8", "Article 8"));
    m_wiki->addRecentChange(makeRC(1009, "2005-01-01T00:00:09Z", "User 9", "Article 9"));
    Date::setFrozenValueOfNow(Date::fromISO8601("2005-01-01T00:00:10Z"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    checkRCRows({
        "1001|2005-01-01T00:00:01Z|edit|Article 1|User 1|||",
        "1002|2005-01-01T00:00:02Z|edit|Article 2|User 2|||",
        "1003|2005-01-01T00:00:03Z|edit|Article 3|User 3|||",
        "1004|2005-01-01T00:00:05Z|edit|Article 5|User 5|||",
    });
  }

  CBL_TEST_CASE(WithFileOnDisk) {
    cbl::TempDir tempDir;
    string dbPath = tempDir.path() + "/recentchanges.sqlite";

    m_wiki = std::make_unique<RCSyncMockWiki>();
    m_wiki->addRecentChange(makeRC(1001, "2005-01-01T00:01:00Z", "User 1", "Article 1"));
    RecentChangesSync(dbPath).updateDatabaseFromWiki(*m_wiki);
    m_recentChangesSync = std::make_unique<RecentChangesSync>(dbPath);
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);

    m_wiki = std::make_unique<RCSyncMockWiki>();
    m_wiki->addRecentChange(makeRC(1002, "2005-01-01T00:02:00Z", "User 2", "Article 2"));
    m_recentChangesSync = std::make_unique<RecentChangesSync>(dbPath);
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);

    checkRCRows({
        "1001|2005-01-01T00:01:00Z|edit|Article 1|User 1|||",
        "1002|2005-01-01T00:02:00Z|edit|Article 2|User 2|||",
    });
  }

  unique_ptr<RCSyncMockWiki> m_wiki;
  unique_ptr<RecentChangesSync> m_recentChangesSync;
};

}  // namespace live_replication

int main() {
  sqlite::Database::forceSynchronousModeOff();
  live_replication::RecentChangesSyncTest().run();
  return 0;
}
