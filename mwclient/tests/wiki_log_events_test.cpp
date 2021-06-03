#include <vector>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/unittest.h"
#include "mwclient/wiki.h"
#include "replay_wiki.h"

using cbl::Date;
using std::vector;

namespace mwc {

class LogEventsTest : public cbl::Test {
public:
  LogEventsTest() : m_replayWiki("log_events") {}

private:
  CBL_TEST_CASE(TimeRange) {
    TestCaseRecord record(m_replayWiki, "TimeRange");
    LogEventsParams params;
    params.prop = RP_TITLE | RP_TIMESTAMP;
    params.start = Date::fromISO8601("2021-05-21T13:37:50Z");
    params.end = Date::fromISO8601("2021-05-21T13:35:54Z");
    params.limit = PAGER_ALL;
    vector<LogEvent> logEvents = m_replayWiki.getLogEvents(params);

    CBL_ASSERT_EQ(logEvents.size(), 6U);
    CBL_ASSERT_EQ(logEvents[0].type(), LE_MOVE);
    CBL_ASSERT_EQ(logEvents[0].title, "Utilisateur:Boolette/Brouillon");
    CBL_ASSERT_EQ(logEvents[0].moveParams().newTitle, "Château de Fretaise");
    CBL_ASSERT_EQ(logEvents[0].timestamp, Date::fromISO8601("2021-05-21T13:37:50Z"));
    CBL_ASSERT_EQ(logEvents[1].type(), LE_PATROL);
    CBL_ASSERT_EQ(logEvents[1].title, "Tour de Bois-Ruffin");
    CBL_ASSERT_EQ(logEvents[1].timestamp, Date::fromISO8601("2021-05-21T13:37:07Z"));
    CBL_ASSERT_EQ(logEvents[2].type(), LE_NEWUSERS);
    CBL_ASSERT_EQ(logEvents[2].title, "Utilisateur:Kuroshide");
    CBL_ASSERT_EQ(logEvents[2].timestamp, Date::fromISO8601("2021-05-21T13:36:49Z"));
    CBL_ASSERT_EQ(logEvents[3].type(), LE_DELETE);
    CBL_ASSERT_EQ(logEvents[3].title, "Syndrome Z");
    CBL_ASSERT_EQ(logEvents[3].timestamp, Date::fromISO8601("2021-05-21T13:36:25Z"));
    CBL_ASSERT_EQ(logEvents[4].type(), LE_PROTECT);
    CBL_ASSERT_EQ(logEvents[4].title, "Traductions de la Bible en français");
    CBL_ASSERT_EQ(logEvents[4].timestamp, Date::fromISO8601("2021-05-21T13:36:19Z"));
    CBL_ASSERT_EQ(logEvents[5].type(), LE_BLOCK);
    CBL_ASSERT_EQ(logEvents[5].title, "Utilisateur:Pololo332311");
    CBL_ASSERT_EQ(logEvents[5].timestamp, Date::fromISO8601("2021-05-21T13:35:54Z"));
  }
  CBL_TEST_CASE(FilterByType) {
    TestCaseRecord record(m_replayWiki, "FilterByType");
    LogEventsParams params;
    params.prop = RP_TITLE;
    params.type = LE_DELETE;
    params.start = Date::fromISO8601("2021-05-30T06:00:00Z");
    params.limit = 3;
    vector<LogEvent> logEvents = m_replayWiki.getLogEvents(params);

    CBL_ASSERT_EQ(logEvents.size(), 3U);
    CBL_ASSERT_EQ(logEvents[0].title, "I Vitelloni");
    CBL_ASSERT_EQ(logEvents[1].title, "I Clowns");
    CBL_ASSERT_EQ(logEvents[2].title, "Catégorie:Roman de la série Fantômette");
  }
  CBL_TEST_CASE(FilterByUser) {
    TestCaseRecord record(m_replayWiki, "FilterByUser");
    LogEventsParams params;
    params.prop = RP_TITLE;
    params.limit = 2;
    params.start = Date::fromISO8601("2021-05-29T23:23:00Z");
    params.user = "OrlodrimBot";
    vector<LogEvent> logEvents = m_replayWiki.getLogEvents(params);
    CBL_ASSERT_EQ(logEvents.size(), 2U);
    CBL_ASSERT_EQ(logEvents[0].type(), LE_MOVE);
    CBL_ASSERT_EQ(logEvents[0].title, "Discussion:Cryptologie et littérature/À faire");
    CBL_ASSERT_EQ(logEvents[1].type(), LE_CREATE);
    CBL_ASSERT_EQ(logEvents[1].title, "Projet:Technologies/Évaluation/Index/7");
  }
  CBL_TEST_CASE(FilterByTitle) {
    TestCaseRecord record(m_replayWiki, "FilterByTitle");
    LogEventsParams params;
    params.prop = RP_TIMESTAMP;
    params.limit = 2;
    params.start = Date::fromISO8601("2016-03-01T00:00:00Z");
    params.title = "Utilisateur:OrlodrimBot";
    vector<LogEvent> logEvents = m_replayWiki.getLogEvents(params);
    CBL_ASSERT_EQ(logEvents.size(), 2U);
    CBL_ASSERT_EQ(logEvents[0].type(), LE_PATROL);
    CBL_ASSERT_EQ(logEvents[0].timestamp, Date::fromISO8601("2016-02-19T15:15:13Z"));
    CBL_ASSERT_EQ(logEvents[1].type(), LE_RIGHTS);
    CBL_ASSERT_EQ(logEvents[1].timestamp, Date::fromISO8601("2013-06-11T08:32:38Z"));
  }
  CBL_TEST_CASE(MoveParams) {
    TestCaseRecord record(m_replayWiki, "MoveParams");
    LogEventsParams params;
    params.prop = RP_TITLE;
    params.type = LE_MOVE;
    params.start = Date::fromISO8601("2021-05-29T23:23:00Z");
    params.limit = 2;
    vector<LogEvent> logEvents = m_replayWiki.getLogEvents(params);

    CBL_ASSERT_EQ(logEvents.size(), 2U);
    CBL_ASSERT_EQ(logEvents[0].type(), LE_MOVE);
    CBL_ASSERT_EQ(logEvents[0].title, "Discussion:Cryptologie et littérature/À faire");
    CBL_ASSERT_EQ(logEvents[0].moveParams().newTitle, "Discussion:Cryptologie dans la littérature/À faire");
    CBL_ASSERT_EQ(logEvents[0].moveParams().suppressRedirect, true);
    CBL_ASSERT_EQ(logEvents[1].type(), LE_MOVE);
    CBL_ASSERT_EQ(logEvents[1].title, "Discussion:Shinobi: Heart Under Blade");
    CBL_ASSERT_EQ(logEvents[1].moveParams().newTitle, "Discussion:Shinobi (film, 2005)");
    CBL_ASSERT_EQ(logEvents[1].moveParams().suppressRedirect, false);
  }

  ReplayWiki m_replayWiki;
};

}  // namespace mwc

int main() {
  mwc::LogEventsTest().run();
  return 0;
}
