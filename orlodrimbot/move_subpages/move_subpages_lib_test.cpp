#include "move_subpages_lib.h"
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "cbl/unittest.h"
#include "mwclient/mock_wiki.h"
#include "mwclient/wiki.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::LogEvent;
using std::string;
using std::unordered_map;
using std::vector;

const Date START_DATE = Date::fromISO8601("2000-01-01T00:00:00Z");

LogEvent createMoveEvent(const string& oldTitle, const string& newTitle, int time) {
  LogEvent logEvent;
  logEvent.setType(mwc::LE_MOVE);
  logEvent.title = oldTitle;
  logEvent.mutableMoveParams().newTitle = newTitle;
  logEvent.timestamp = START_DATE + DateDiff::fromHours(time);
  return logEvent;
}

class MockWikiForSubpagesMover : public mwc::MockWiki {
public:
  vector<LogEvent> getLogEvents(const mwc::LogEventsParams& params) override { return logEvents; }
  void movePage(const string& oldTitle, const string& newTitle, const string& summary, int flags) override {
    if (!moves.empty()) moves += "|";
    cbl::append(moves, oldTitle, " -> ", newTitle, flags & mwc::MOVE_NOREDIRECT ? " [noredirect]" : "");
  }
  unordered_map<string, bool> getPagesDisambigStatus(const vector<string>& titles) override {
    unordered_map<string, bool> result;
    for (const string& title : titles) {
      result[title] = readPageContent(title).find("{{Homonymie}}") != string::npos;
    }
    return result;
  }
  vector<string> getBacklinks(const mwc::BacklinksParams& params) override { return backlinks[params.title]; }

  vector<LogEvent> logEvents;
  unordered_map<string, vector<string>> backlinks;
  string moves;
};

class SubpagesMoverTest : public cbl::Test {
private:
  CBL_TEST_CASE(StandardMove) {
    MockWikiForSubpagesMover wiki;
    wiki.setPageContent("Page 1", "#redirect[[Page 2]]");
    wiki.setPageContent("Discussion:Page 1/Archive", "Talk page archive");
    wiki.setPageContent("Discussion:Page 1/Archive Commons", "Commons deletions archive");
    wiki.setPageContent("Discussion:Page 1/À faire", "Some tasks");
    wiki.setPageContent("Page 2", "Article content");
    wiki.logEvents = {createMoveEvent("Page 1", "Page 2", 0)};
    SubpagesMover subpagesMover(&wiki, START_DATE, false);
    subpagesMover.processAllMoves();
    CBL_ASSERT_EQ(wiki.moves,
                  "Discussion:Page 1/Archive -> Discussion:Page 2/Archive|"
                  "Discussion:Page 1/Archive Commons -> Discussion:Page 2/Archive Commons [noredirect]|"
                  "Discussion:Page 1/À faire -> Discussion:Page 2/À faire [noredirect]");
  }
  CBL_TEST_CASE(StandardMoveWithoutRedirect) {
    MockWikiForSubpagesMover wiki;
    wiki.setPageContent("Discussion:Page 1/Archive", "Talk page archive");
    wiki.setPageContent("Page 2", "Article content");
    wiki.logEvents = {createMoveEvent("Page 1", "Page 2", 0)};
    SubpagesMover subpagesMover(&wiki, START_DATE, false);
    subpagesMover.processAllMoves();
    CBL_ASSERT_EQ(wiki.moves, "Discussion:Page 1/Archive -> Discussion:Page 2/Archive");
  }
  CBL_TEST_CASE(ConsecutiveMoves) {
    MockWikiForSubpagesMover wiki;
    wiki.setPageContent("Page 1", "#redirect[[Page 3]]");
    wiki.setPageContent("Discussion:Page 1/Archive", "Talk page archive");
    wiki.setPageContent("Page 2", "#redirect[[Page 3]]");
    wiki.setPageContent("Page 3", "Article content");
    wiki.logEvents = {createMoveEvent("Page 2", "Page 3", 1), createMoveEvent("Page 1", "Page 2", 0)};
    SubpagesMover subpagesMover(&wiki, START_DATE, false);
    subpagesMover.processAllMoves();
    CBL_ASSERT_EQ(wiki.moves, "Discussion:Page 1/Archive -> Discussion:Page 3/Archive");
  }
  CBL_TEST_CASE(ConsecutiveMovesWithSubpagesMovedOnce) {
    MockWikiForSubpagesMover wiki;
    wiki.setPageContent("Page 1", "#redirect[[Page 3]]");
    wiki.setPageContent("Page 2", "#redirect[[Page 3]]");
    wiki.setPageContent("Discussion:Page 2/Archive", "Talk page archive");
    wiki.setPageContent("Page 3", "Article content");
    wiki.logEvents = {createMoveEvent("Page 2", "Page 3", 1), createMoveEvent("Page 1", "Page 2", 0)};
    SubpagesMover subpagesMover(&wiki, START_DATE, false);
    subpagesMover.processAllMoves();
    CBL_ASSERT_EQ(wiki.moves, "Discussion:Page 2/Archive -> Discussion:Page 3/Archive");
  }
  CBL_TEST_CASE(ConsecutiveMovesWithNamespaceTemporarilyChanged) {
    MockWikiForSubpagesMover wiki;
    wiki.setPageContent("Page 1", "#redirect[[Page 3]]");
    wiki.setPageContent("Discussion:Page 1/Archive", "Talk page archive");
    wiki.setPageContent("Aide:Page 2", "#redirect[[Page 3]]");
    wiki.setPageContent("Page 3", "Article content");
    wiki.logEvents = {createMoveEvent("Aide:Page 2", "Page 3", 1), createMoveEvent("Page 1", "Aide:Page 2", 0)};
    SubpagesMover subpagesMover(&wiki, START_DATE, false);
    subpagesMover.processAllMoves();
    CBL_ASSERT_EQ(wiki.moves, "Discussion:Page 1/Archive -> Discussion:Page 3/Archive");
  }
  CBL_TEST_CASE(NamespaceFiltering) {
    MockWikiForSubpagesMover wiki;
    wiki.setPageContent("Discussion utilisateur:Page 1/Archive", "Talk page archive");
    wiki.setPageContent("Utilisateur:Page 2", "User page content");
    wiki.setPageContent("Discussion aide:Page 3/Archive", "Talk page archive");
    wiki.setPageContent("Aide:Page 4", "Help page content");
    wiki.logEvents = {
        createMoveEvent("Utilisateur:Page 1", "Utilisateur:Page 2", 1),
        createMoveEvent("Aide:Page 3", "Aide:Page 4", 0),
    };
    SubpagesMover subpagesMover(&wiki, START_DATE, false);
    subpagesMover.processAllMoves();
    CBL_ASSERT_EQ(wiki.moves, "Discussion aide:Page 3/Archive -> Discussion aide:Page 4/Archive");
  }
  CBL_TEST_CASE(DifferentNamespaces) {
    MockWikiForSubpagesMover wiki;
    wiki.setPageContent("Discussion:Page 1/Archive", "Talk page archive");
    wiki.setPageContent("Aide:Page 2", "Help page content");
    wiki.logEvents = {createMoveEvent("Page 1", "Aide:Page 2", 0)};
    SubpagesMover subpagesMover(&wiki, START_DATE, false);
    subpagesMover.processAllMoves();
    CBL_ASSERT_EQ(wiki.moves, "");
  }
  CBL_TEST_CASE(OldPageStillExists) {
    MockWikiForSubpagesMover wiki;
    wiki.setPageContent("Page 1", "Article content 1");
    wiki.setPageContent("Discussion:Page 1/Archive", "Talk page archive");
    wiki.setPageContent("Page 2", "Article content 2");
    wiki.logEvents = {createMoveEvent("Page 1", "Page 2", 0)};
    SubpagesMover subpagesMover(&wiki, START_DATE, false);
    subpagesMover.processAllMoves();
    CBL_ASSERT_EQ(wiki.moves, "");
  }
  CBL_TEST_CASE(OldPageStillExistsButIsDisambig) {
    MockWikiForSubpagesMover wiki;
    wiki.setPageContent("Page 1", "{{Homonymie}}");
    wiki.setPageContent("Discussion:Page 1/Archive", "Talk page archive");
    wiki.setPageContent("Page 2", "Article content 2");
    wiki.logEvents = {createMoveEvent("Page 1", "Page 2", 0)};
    SubpagesMover subpagesMover(&wiki, START_DATE, false);
    subpagesMover.processAllMoves();
    CBL_ASSERT_EQ(wiki.moves, "Discussion:Page 1/Archive -> Discussion:Page 2/Archive");
  }
  CBL_TEST_CASE(NewPageWasDeleted) {
    MockWikiForSubpagesMover wiki;
    wiki.setPageContent("Page 1", "{{Homonymie}}");
    wiki.setPageContent("Discussion:Page 1/Archive", "Talk page archive");
    wiki.logEvents = {createMoveEvent("Page 1", "Page 2", 0)};
    SubpagesMover subpagesMover(&wiki, START_DATE, false);
    subpagesMover.processAllMoves();
    CBL_ASSERT_EQ(wiki.moves, "");
  }
  CBL_TEST_CASE(NonStandardSubpage) {
    MockWikiForSubpagesMover wiki;
    wiki.setPageContent("AC (homonymie)", "Some content");
    wiki.setPageContent("Discussion:AC/DC", "Talk page of AC/DC");
    wiki.logEvents = {createMoveEvent("AC", "AC (homonymie)", 0)};
    SubpagesMover subpagesMover(&wiki, START_DATE, false);
    subpagesMover.processAllMoves();
    CBL_ASSERT_EQ(wiki.moves, "");
  }
  CBL_TEST_CASE(CheckBacklinks) {
    MockWikiForSubpagesMover wiki;
    wiki.setPageContent("Discussion:Page 1/Archive Commons", "Commons deletions archive");
    wiki.setPageContent("Page 2", "Article content");
    wiki.setPageContent("Discussion:Page 3/Archive Commons", "Commons deletions archive");
    wiki.setPageContent("Page 4", "Article content");
    wiki.logEvents = {createMoveEvent("Page 1", "Page 2", 1), createMoveEvent("Page 3", "Page 4", 0)};
    wiki.backlinks["Discussion:Page 3/Archive Commons"].push_back("Some page");
    SubpagesMover subpagesMover(&wiki, START_DATE, false);
    subpagesMover.processAllMoves();
    CBL_ASSERT_EQ(wiki.moves,
                  "Discussion:Page 1/Archive Commons -> Discussion:Page 2/Archive Commons [noredirect]|"
                  "Discussion:Page 3/Archive Commons -> Discussion:Page 4/Archive Commons");
  }
};

int main() {
  SubpagesMoverTest().run();
  return 0;
}
