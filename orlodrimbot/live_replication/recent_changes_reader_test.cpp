#include "recent_changes_reader.h"
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/sqlite.h"
#include "cbl/string.h"
#include "cbl/tempfile.h"
#include "cbl/unittest.h"
#include "mwclient/wiki.h"
#include "recent_changes_sync.h"
#include "recent_changes_test_util.h"

using cbl::Date;
using cbl::TempDir;
using mwc::LogEvent;
using mwc::RecentChange;
using std::string;
using std::unique_ptr;
using std::unordered_set;
using std::vector;

namespace live_replication {

string debugString(const unordered_set<string>& container) {
  vector<string> sortedItems(container.begin(), container.end());
  std::sort(sortedItems.begin(), sortedItems.end());
  return cbl::join(sortedItems, ",");
}

class RecentChangesReaderTest : public cbl::Test {
public:
  void setUp() override {
    m_tempDir = std::make_unique<TempDir>();
    m_dbPath = m_tempDir->path() + "/recentchanges.sqlite";
    m_wiki = std::make_unique<RCSyncMockWiki>();
    m_recentChangesSync = std::make_unique<RecentChangesSync>(m_dbPath);
    m_wiki->addRecentChange(makeRC(1000, "2000-01-01T00:00:00Z", "Change0-user", "Change0-title"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
  }

  CBL_TEST_CASE(enumRecentChanges_multipleUpdates) {
    vector<RecentChange> recentChanges;
    auto processRecentChange = [&](const RecentChange& recentChange) { recentChanges.push_back(recentChange.copy()); };

    RecentChangesReader recentChangesReader(m_dbPath);
    string continueToken;
    recentChangesReader.enumRecentChanges({.continueToken = &continueToken}, processRecentChange);
    CBL_ASSERT(!continueToken.empty());
    CBL_ASSERT(recentChanges.empty());

    m_wiki->addRecentChange(makeRC(1001, "2000-01-01T00:01:00Z", "User 1", "Article 1"));
    m_wiki->addRecentChange(
        makeLogRC(1002, 501, mwc::LE_MOVE, "move", "2000-01-01T00:02:00Z", "User 2", "Article 2", "Article 2 renamed"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    string continueToken2 = continueToken;
    recentChangesReader.enumRecentChanges({.continueToken = &continueToken2}, processRecentChange);
    CBL_ASSERT(continueToken != continueToken2) << continueToken;
    CBL_ASSERT_EQ(recentChanges.size(), 2U);
    CBL_ASSERT_EQ(recentChanges[0].type(), mwc::RC_EDIT);
    CBL_ASSERT_EQ(recentChanges[0].timestamp(), Date::fromISO8601("2000-01-01T00:01:00Z"));
    CBL_ASSERT_EQ(recentChanges[0].user(), "User 1");
    CBL_ASSERT_EQ(recentChanges[0].title(), "Article 1");
    CBL_ASSERT_EQ(recentChanges[1].type(), mwc::RC_LOG);
    CBL_ASSERT_EQ(recentChanges[1].timestamp(), Date::fromISO8601("2000-01-01T00:02:00Z"));
    CBL_ASSERT_EQ(recentChanges[1].user(), "User 2");
    CBL_ASSERT_EQ(recentChanges[1].title(), "Article 2");
    CBL_ASSERT_EQ(recentChanges[1].logEvent().newTitle(), "Article 2 renamed");

    string continueToken3 = continueToken2;
    recentChangesReader.enumRecentChanges({.continueToken = &continueToken3}, processRecentChange);
    CBL_ASSERT_EQ(continueToken2, continueToken3);
    CBL_ASSERT_EQ(recentChanges.size(), 2U);

    m_wiki->addRecentChange(makeRC(1003, "2000-01-01T00:03:00Z", "User 3", "Article 3"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    recentChangesReader.enumRecentChanges({.continueToken = &continueToken3}, processRecentChange);
    CBL_ASSERT_EQ(recentChanges.size(), 3U);
    CBL_ASSERT_EQ(recentChanges[2].title(), "Article 3");
  }

  CBL_TEST_CASE(enumRecentChanges_subsetOfProperties) {
    vector<RecentChange> recentChanges;
    auto processRecentChange = [&](const RecentChange& recentChange) { recentChanges.push_back(recentChange.copy()); };

    RecentChangesReader recentChangesReader(m_dbPath);
    string continueToken;
    recentChangesReader.enumRecentChanges({.properties = mwc::RP_TIMESTAMP, .continueToken = &continueToken},
                                          processRecentChange);
    string oldContinueToken = continueToken;

    RecentChange rc1001 = makeRC(1001, "2000-01-01T00:01:00Z", "User 1", "Article 1");
    rc1001.mutableRevision().size = 12345;
    rc1001.mutableRevision().comment = "Test edit comment";
    rc1001.oldRevid = 50;
    rc1001.mutableRevision().revid = 60;
    m_wiki->addRecentChange(std::move(rc1001));
    RecentChange rc1002 = makeLogRC(1002, 501, mwc::LE_DELETE, "delete", "2000-01-01T00:02:00Z", "User 2", "Article 2");
    rc1002.mutableLogEvent().comment = "Test log comment";
    rc1002.mutableLogEvent().logid = 30;
    m_wiki->addRecentChange(std::move(rc1002));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);

    recentChangesReader.enumRecentChanges({.properties = mwc::RP_TIMESTAMP, .continueToken = &continueToken},
                                          processRecentChange);
    CBL_ASSERT_EQ(recentChanges.size(), 2U);
    CBL_ASSERT_EQ(recentChanges[0].timestamp(), Date::fromISO8601("2000-01-01T00:01:00Z"));
    CBL_ASSERT_EQ(recentChanges[0].user(), "");
    CBL_ASSERT_EQ(recentChanges[0].title(), "");
    CBL_ASSERT_EQ(recentChanges[0].type(), mwc::RC_EDIT);
    CBL_ASSERT_EQ(recentChanges[0].oldRevid, 0);
    CBL_ASSERT_EQ(recentChanges[0].revision().size, 0);
    CBL_ASSERT_EQ(recentChanges[0].revision().comment, "");
    CBL_ASSERT_EQ(recentChanges[0].revision().revid, 0);
    CBL_ASSERT_EQ(recentChanges[1].timestamp(), Date::fromISO8601("2000-01-01T00:02:00Z"));
    CBL_ASSERT_EQ(recentChanges[1].user(), "");
    CBL_ASSERT_EQ(recentChanges[1].title(), "");
    CBL_ASSERT_EQ(recentChanges[1].type(), mwc::RC_LOG);
    CBL_ASSERT_EQ(recentChanges[1].logEvent().logid, 0);

    continueToken = oldContinueToken;
    recentChanges.clear();
    recentChangesReader.enumRecentChanges(
        {.properties = mwc::RP_SIZE | mwc::RP_COMMENT | mwc::RP_REVID, .continueToken = &continueToken},
        processRecentChange);
    CBL_ASSERT_EQ(recentChanges.size(), 2U);
    CBL_ASSERT(recentChanges[0].timestamp().isNull());
    CBL_ASSERT_EQ(recentChanges[0].oldRevid, 50);
    CBL_ASSERT_EQ(recentChanges[0].type(), mwc::RC_EDIT);
    CBL_ASSERT_EQ(recentChanges[0].revision().size, 12345);
    CBL_ASSERT_EQ(recentChanges[0].revision().comment, "Test edit comment");
    CBL_ASSERT_EQ(recentChanges[0].revision().revid, 60);
    CBL_ASSERT_EQ(recentChanges[1].type(), mwc::RC_LOG);
    CBL_ASSERT_EQ(recentChanges[1].logEvent().comment, "Test log comment");
    CBL_ASSERT_EQ(recentChanges[1].logEvent().logid, 30);
  }

  CBL_TEST_CASE(enumRecentChanges_withStart) {
    m_wiki->addRecentChange(makeRC(1001, "2000-01-01T00:01:00Z", "User 1", "Article 1"));
    m_wiki->addRecentChange(makeRC(1002, "2000-01-01T00:02:00Z", "User 2", "Article 2"));
    m_wiki->addRecentChange(makeRC(1003, "2000-01-01T00:03:00Z", "User 3", "Article 3"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);

    // start set, continueToken not set.
    vector<RecentChange> recentChanges;
    auto processRecentChange = [&](const RecentChange& recentChange) { recentChanges.push_back(recentChange.copy()); };
    RecentChangesReader recentChangesReader(m_dbPath);
    RecentChangesOptions options;
    options.start = Date::fromISO8601("2000-01-01T00:02:00Z");
    recentChangesReader.enumRecentChanges(options, processRecentChange);
    CBL_ASSERT_EQ(recentChanges.size(), 2U);
    CBL_ASSERT_EQ(recentChanges[0].title(), "Article 2");
    CBL_ASSERT_EQ(recentChanges[1].title(), "Article 3");

    // start to another value, continueToken not set.
    recentChanges.clear();
    options.start = Date::fromISO8601("2000-01-01T00:02:01Z");
    recentChangesReader.enumRecentChanges(options, processRecentChange);
    CBL_ASSERT_EQ(recentChanges.size(), 1U);
    CBL_ASSERT_EQ(recentChanges[0].title(), "Article 3");

    // start and continueToken both set. start points to the most recent change.
    string continueToken = "rc|1002";
    recentChanges.clear();
    options.continueToken = &continueToken;
    options.start = Date::fromISO8601("2000-01-01T00:03:00Z");
    recentChangesReader.enumRecentChanges(options, processRecentChange);
    CBL_ASSERT_EQ(recentChanges.size(), 1U);
    CBL_ASSERT_EQ(recentChanges[0].title(), "Article 3");

    // start and continueToken both set. continueToken points to the most recent change.
    continueToken = "rc|1002";
    recentChanges.clear();
    options.continueToken = &continueToken;
    options.start = Date::fromISO8601("2000-01-01T00:01:00Z");
    recentChangesReader.enumRecentChanges(options, processRecentChange);
    CBL_ASSERT_EQ(recentChanges.size(), 2U);
    CBL_ASSERT_EQ(recentChanges[0].title(), "Article 2");
    CBL_ASSERT_EQ(recentChanges[1].title(), "Article 3");
  }

  CBL_TEST_CASE(enumRecentChanges_withEnd) {
    vector<RecentChange> recentChanges;
    auto processRecentChange = [&](const RecentChange& recentChange) { recentChanges.push_back(recentChange.copy()); };
    RecentChangesReader recentChangesReader(m_dbPath);

    string continueToken;
    RecentChangesOptions options;
    options.continueToken = &continueToken;
    recentChangesReader.enumRecentChanges({.continueToken = &continueToken}, processRecentChange);
    CBL_ASSERT(recentChanges.empty());

    m_wiki->addRecentChange(makeRC(1001, "2000-01-01T00:01:00Z", "User 1", "Article 1"));
    m_wiki->addRecentChange(makeRC(1002, "2000-01-01T00:02:00Z", "User 2", "Article 2"));
    m_wiki->addRecentChange(makeRC(1003, "2000-01-01T00:03:00Z", "User 3", "Article 3"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);

    options.end = Date::fromISO8601("2000-01-01T00:02:00Z");
    recentChangesReader.enumRecentChanges(options, processRecentChange);
    CBL_ASSERT_EQ(recentChanges.size(), 2U);
    CBL_ASSERT_EQ(recentChanges[0].title(), "Article 1");
    CBL_ASSERT_EQ(recentChanges[1].title(), "Article 2");

    recentChanges.clear();
    options.end = Date();
    recentChangesReader.enumRecentChanges(options, processRecentChange);
    CBL_ASSERT_EQ(recentChanges.size(), 1U);
    CBL_ASSERT_EQ(recentChanges[0].title(), "Article 3");
  }

  CBL_TEST_CASE(enumRecentChanges_withType) {
    vector<RecentChange> recentChanges;
    auto processRecentChange = [&](const RecentChange& recentChange) { recentChanges.push_back(recentChange.copy()); };

    string continueToken;
    RecentChangesReader recentChangesReader(m_dbPath);
    RecentChangesOptions options;
    options.continueToken = &continueToken;
    options.type = mwc::RC_EDIT | mwc::RC_LOG;
    recentChangesReader.enumRecentChanges({.properties = mwc::RP_TIMESTAMP, .continueToken = &continueToken},
                                          processRecentChange);

    m_wiki->addRecentChange(makeRC(1001, "2000-01-01T00:01:00Z", "User 1", "Article 1", mwc::RC_EDIT));
    m_wiki->addRecentChange(makeRC(1002, "2000-01-01T00:02:00Z", "User 2", "Article 2", mwc::RC_NEW));
    m_wiki->addRecentChange(
        makeLogRC(1003, 501, mwc::LE_DELETE, "delete", "2000-01-01T00:03:00Z", "User 3", "Article 3"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    recentChangesReader.enumRecentChanges(options, processRecentChange);
    CBL_ASSERT_EQ(recentChanges.size(), 2U);
    CBL_ASSERT_EQ(recentChanges[0].title(), "Article 1");
    CBL_ASSERT_EQ(recentChanges[1].title(), "Article 3");
  }

  CBL_TEST_CASE(enumRecentChanges_withLimit) {
    m_wiki->addRecentChange(makeRC(1001, "2000-01-01T00:01:00Z", "User 1", "Article 1"));
    m_wiki->addRecentChange(makeRC(1002, "2000-01-01T00:02:00Z", "User 2", "Article 2"));
    m_wiki->addRecentChange(makeRC(1003, "2000-01-01T00:03:00Z", "User 3", "Article 3"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);

    vector<RecentChange> recentChanges;
    auto processRecentChange = [&](const RecentChange& recentChange) { recentChanges.push_back(recentChange.copy()); };
    RecentChangesReader recentChangesReader(m_dbPath);
    string continueToken;
    RecentChangesOptions options;
    options.start = Date::fromISO8601("2000-01-01T00:01:00Z");
    options.limit = 2;
    options.continueToken = &continueToken;
    recentChangesReader.enumRecentChanges(options, processRecentChange);
    CBL_ASSERT_EQ(recentChanges.size(), 2U);
    CBL_ASSERT_EQ(recentChanges[0].title(), "Article 1");
    CBL_ASSERT_EQ(recentChanges[1].title(), "Article 2");
    CBL_ASSERT_EQ(continueToken, "rc|1003");
  }

  CBL_TEST_CASE(getRecentlyUpdatedPages) {
    RecentChangesReader recentChangesReader(m_dbPath);
    string continueToken;
    RecentlyUpdatedPagesOptions options;
    options.continueToken = &continueToken;
    CBL_ASSERT(recentChangesReader.getRecentlyUpdatedPages(options).empty());

    m_wiki->addRecentChange(makeRC(1001, "2000-01-01T00:01:00Z", "User 1", "Article 1"));
    m_wiki->addRecentChange(
        makeLogRC(1002, 501, mwc::LE_MOVE, "move", "2000-01-01T00:02:00Z", "User 2", "Article 2", "Article 2 renamed"));
    // The user may be empty if it is hidden from public view. Having an empty excludedUser should not exclude this.
    m_wiki->addRecentChange(makeRC(1003, "2000-01-01T00:03:00Z", "", "Article 3"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    unordered_set<string> pages = recentChangesReader.getRecentlyUpdatedPages(options);
    CBL_ASSERT_EQ(debugString(pages), "Article 1,Article 2,Article 2 renamed,Article 3");
  }

  CBL_TEST_CASE(getRecentlyUpdatedPages_excludedUser) {
    RecentChangesReader recentChangesReader(m_dbPath);
    string continueToken;
    RecentlyUpdatedPagesOptions options;
    options.continueToken = &continueToken;
    CBL_ASSERT(recentChangesReader.getRecentlyUpdatedPages(options).empty());
    m_wiki->addRecentChange(makeRC(1001, "2000-01-01T00:01:00Z", "User 1", "Article 1"));
    m_wiki->addRecentChange(makeRC(1002, "2000-01-01T00:02:00Z", "User 2", "Article 2"));
    m_wiki->addRecentChange(makeRC(1003, "2000-01-01T00:03:00Z", "User 3", "Article 3"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);
    options.excludedUser = "User 2";
    unordered_set<string> pages = recentChangesReader.getRecentlyUpdatedPages(options);
    CBL_ASSERT_EQ(debugString(pages), "Article 1,Article 3");
  }

  CBL_TEST_CASE(getRecentlyUpdatedPages_withStart) {
    m_wiki->addRecentChange(makeRC(1001, "2000-01-01T00:01:00Z", "User 1", "Article 1"));
    m_wiki->addRecentChange(makeRC(1002, "2000-01-01T00:02:00Z", "User 2", "Article 2"));
    m_wiki->addRecentChange(makeRC(1003, "2000-01-01T00:03:00Z", "User 3", "Article 3"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);

    RecentChangesReader recentChangesReader(m_dbPath);
    RecentlyUpdatedPagesOptions options;
    options.start = Date::fromISO8601("2000-01-01T00:02:00Z");
    unordered_set<string> pages = recentChangesReader.getRecentlyUpdatedPages(options);
    CBL_ASSERT_EQ(debugString(pages), "Article 2,Article 3");
  }

  CBL_TEST_CASE(getRecentLogEvents) {
    RecentChangesReader recentChangesReader(m_dbPath);
    string continueToken;
    RecentLogEventsOptions options;
    options.continueToken = &continueToken;
    CBL_ASSERT(recentChangesReader.getRecentLogEvents(options).empty());

    m_wiki->addRecentChange(makeRC(1001, "2000-01-01T00:01:00Z", "User 1", "Article 1"));
    m_wiki->addRecentChange(
        makeLogRC(1002, 501, mwc::LE_MOVE, "move", "2000-01-01T00:02:00Z", "User 2", "Article 2", "Article 2 renamed"));
    m_wiki->addRecentChange(
        makeLogRC(1003, 502, mwc::LE_DELETE, "delete", "2000-01-01T00:03:00Z", "User 3", "Article 3"));
    m_wiki->addRecentChange(
        makeLogRC(1004, 503, mwc::LE_PROTECT, "protect", "2000-01-01T00:04:00Z", "User 4", "Article 4"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);

    string continueToken2 = continueToken;
    options.continueToken = &continueToken2;
    vector<LogEvent> logEvents = recentChangesReader.getRecentLogEvents(options);
    CBL_ASSERT_EQ(logEvents.size(), 3U);
    CBL_ASSERT_EQ(logEvents[0].title, "Article 2");
    CBL_ASSERT_EQ(logEvents[0].type, mwc::LE_MOVE);
    CBL_ASSERT_EQ(logEvents[1].title, "Article 3");
    CBL_ASSERT_EQ(logEvents[1].type, mwc::LE_DELETE);
    CBL_ASSERT_EQ(logEvents[2].title, "Article 4");
    CBL_ASSERT_EQ(logEvents[2].type, mwc::LE_PROTECT);

    continueToken2 = continueToken;
    options.continueToken = &continueToken2;
    options.logType = mwc::LE_MOVE;
    logEvents = recentChangesReader.getRecentLogEvents(options);
    CBL_ASSERT_EQ(logEvents.size(), 1U);
    CBL_ASSERT_EQ(logEvents[0].title, "Article 2");
  }

  CBL_TEST_CASE(getRecentLogEvents_withStart) {
    m_wiki->addRecentChange(
        makeLogRC(1001, 501, mwc::LE_DELETE, "delete", "2000-01-01T00:01:00Z", "User 1", "Article 1"));
    m_wiki->addRecentChange(
        makeLogRC(1002, 502, mwc::LE_DELETE, "delete", "2000-01-01T00:02:00Z", "User 2", "Article 2"));
    m_recentChangesSync->updateDatabaseFromWiki(*m_wiki);

    RecentChangesReader recentChangesReader(m_dbPath);
    RecentLogEventsOptions options;
    options.start = Date::fromISO8601("2000-01-01T00:02:00Z");
    vector<LogEvent> logEvents = recentChangesReader.getRecentLogEvents(options);
    CBL_ASSERT_EQ(logEvents.size(), 1U);
    CBL_ASSERT_EQ(logEvents[0].title, "Article 2");
  }

  unique_ptr<TempDir> m_tempDir;
  string m_dbPath;
  unique_ptr<RCSyncMockWiki> m_wiki;
  unique_ptr<RecentChangesSync> m_recentChangesSync;
};

}  // namespace live_replication

int main() {
  sqlite::Database::forceSynchronousModeOff();
  live_replication::RecentChangesReaderTest().run();
  return 0;
}
