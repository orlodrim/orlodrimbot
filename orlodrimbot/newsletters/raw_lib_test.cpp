#include "raw_lib.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "cbl/date.h"
#include "cbl/file.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/tempfile.h"
#include "mwclient/mock_wiki.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/mock_recent_changes_reader.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"

using cbl::Date;
using cbl::DateDiff;
using cbl::TempFile;
using mwc::LE_MOVE;
using mwc::LogEvent;
using mwc::MockWiki;
using mwc::RCM_FLOW_BOARD;
using mwc::Revision;
using mwc::RP_CONTENT_MODEL;
using mwc::UG_AUTOPATROLLED;
using mwc::UIP_GROUPS;
using mwc::UserContribsParams;
using mwc::UserInfo;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::vector;

namespace {

class MockRCReaderWithMoves : public live_replication::MockRecentChangesReader {
public:
  vector<LogEvent> getRecentLogEvents(const live_replication::RecentLogEventsOptions& options) override {
    CBL_ASSERT(options.continueToken != nullptr);
    vector<LogEvent> moves;
    Date start;
    if (options.continueToken->empty()) {
      start = options.start;
    } else {
      CBL_ASSERT(options.continueToken->starts_with("token:"));
      start = Date::fromISO8601(options.continueToken->substr(strlen("token:")));
    }
    CBL_ASSERT(!start.isNull());
    for (const LogEvent& move : m_moves) {
      if (move.timestamp >= start) {
        moves.push_back(move);
      }
    }
    std::sort(moves.begin(), moves.end(),
              [](const LogEvent& move1, const LogEvent& move2) { return move1.timestamp < move2.timestamp; });
    if (!moves.empty()) {
      *options.continueToken = "token:" + (moves.back().timestamp + DateDiff::fromSeconds(1)).toISO8601();
    }
    return moves;
  }

  void addMove(const string& source, const string& target, const Date& timestamp, const string& user) {
    LogEvent logEvent;
    logEvent.setType(LE_MOVE);
    logEvent.title = source;
    logEvent.mutableMoveParams().newTitle = target;
    logEvent.timestamp = timestamp;
    logEvent.user = user;
    m_moves.push_back(logEvent);
  }
  void reset() { m_moves.clear(); }

private:
  vector<LogEvent> m_moves;
};

class MyMockWiki : public MockWiki {
public:
  using MockWiki::readPage;

  void getUsersInfo(int properties, vector<UserInfo>& users) override {
    CBL_ASSERT_EQ(properties, UIP_GROUPS);
    for (UserInfo& user : users) {
      user.groups = user.name.starts_with("Trusted") ? UG_AUTOPATROLLED : 0;
    }
  }
  vector<Revision> getUserContribs(const UserContribsParams& params) override {
    vector<Revision> result;
    if (params.user.find("WithContribs") != string::npos) {
      result.emplace_back();
    }
    return result;
  }
  Revision readPage(string_view title, int properties) override {
    Revision revision = MockWiki::readPage(title, properties);
    if ((properties & RP_CONTENT_MODEL) && title.find(":Flow") != string_view::npos) {
      revision.contentModel = RCM_FLOW_BOARD;
    }
    return revision;
  }
  void flowNewTopic(const string& title, const string& topic, const string& content, int flags = 0) override {
    setPageContent(title, readPageContent(title) + "\nFLOW|" + topic + "|" + content);
  }
};

class RAWDistributorTest {
public:
  void run();

private:
  void reset(const string& state = "{}");
  void testStandardDistribution();
  void testRemoveOldMessages();
  void testMoveFiltering();
  void testNewsletterContentFiltering();
  void testTargetPageFiltering();
  void testFlow();

  TempFile m_stateFile;
  MyMockWiki m_wiki;
  MockRCReaderWithMoves m_recentChangesReader;
  unique_ptr<RAWDistributor> m_rawDistributor;
  const string m_goodIssueContent = "{{RAW/En-tête\n|numéro=123\n}}\n" + string(300, '.');
};

#define RUN_TEST(testName)           \
  CBL_INFO << "[test" #testName "]"; \
  reset();                           \
  test##testName();

void RAWDistributorTest::run() {
  RUN_TEST(StandardDistribution);
  RUN_TEST(RemoveOldMessages);
  RUN_TEST(MoveFiltering);
  RUN_TEST(NewsletterContentFiltering);
  RUN_TEST(TargetPageFiltering);
  RUN_TEST(Flow);
}

void RAWDistributorTest::reset(const string& state) {
  Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T00:00:00Z"));
  cbl::writeFile(m_stateFile.path(), state);
  m_recentChangesReader.reset();
  m_wiki.resetDatabase();
  m_rawDistributor.reset(new RAWDistributor(&m_wiki, m_stateFile.path(), &m_recentChangesReader));
}

void RAWDistributorTest::testStandardDistribution() {
  const char TWEETS_PAGE[] = "Wikipédia:Réseaux sociaux/Publications";
  m_wiki.setPageContent("Wikipédia:RAW/2000-01-01", m_goodIssueContent);
  m_wiki.setPageContent(TWEETS_PAGE, "== 2 janvier ==");
  m_wiki.setPageContent("Discussion utilisateur:TestUser1",
                        "Header 1\n\n"
                        "== [[Wikipédia:RAW/1999-12-01|RAW 1999-12-01]] ==\n"
                        "{{RAW/Distribution|1999-12-01}} ~~~~\n\n"
                        "== Test section ==\n\n"
                        "== [[Wikipédia:RAW/1999-12-15|RAW 1999-12-15]] ==\n"
                        "{{RAW/Distribution|1999-12-15}} ~~~~");
  m_wiki.setPageContent("Discussion utilisateur:TestUser2", "Header 2");
  m_wiki.setPageContent("Discussion Projet:MyProject", "Header 3");
  m_wiki.setPageContent("Wikipédia:Le Bistro/1 janvier 2000", "Header 4");
  m_recentChangesReader.addMove("Wikipédia:RAW/Rédaction", "Wikipédia:RAW/2000-01-01",
                                Date::fromISO8601("2000-01-01T10:00:00Z"), "TrustedUser");
  Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T10:02:00Z"));
  m_wiki.setPageContent("Wikipédia:RAW/Inscription",
                        "*{{#target:User:TestUser1|fr.wikipedia.org}}\n"
                        "*{{#target:User:TestUser2|fr.wikipedia.org}}\n"
                        "*{{#target:Discussion Projet:MyProject|fr.wikipedia.org}}\n"
                        "*{{Abonnement Bistro}}");
  CBL_ASSERT(m_rawDistributor->run("", "", "", false, false));
  CBL_ASSERT_EQ(m_wiki.readPageContent(TWEETS_PAGE),
                "== 2 janvier ==\n"
                "{{Proposition tweet\n"
                "|texte=Le n° 123 des « Regards sur l'actualité de la Wikimedia » est sorti : "
                "https://fr.wikipedia.org/wiki/Wikipédia:RAW/2000-01-01\n"
                "|média=Proposition Washington.svg\n"
                "|mode=bot\n"
                "|proposé par=~~~~\n"
                "|validé par=\n"
                "|publié par=\n"
                "}}");
  CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:TestUser1"),
                "Header 1\n\n"
                "== Test section ==\n\n"
                "== [[Wikipédia:RAW/1999-12-15|RAW 1999-12-15]] ==\n"
                "{{RAW/Distribution|1999-12-15}} ~~~~\n\n"
                "== [[Wikipédia:RAW/2000-01-01|RAW 2000-01-01]] ==\n"
                "{{RAW/Distribution|2000-01-01}} ~~~~");
  CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:TestUser2"),
                "Header 2\n\n"
                "== [[Wikipédia:RAW/2000-01-01|RAW 2000-01-01]] ==\n"
                "{{RAW/Distribution|2000-01-01}} ~~~~");
  CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion Projet:MyProject"),
                "Header 3\n\n"
                "== [[Wikipédia:RAW/2000-01-01|RAW 2000-01-01]] ==\n"
                "{{RAW/Distribution|2000-01-01}} ~~~~");
  CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Le Bistro/1 janvier 2000"),
                "Header 4\n\n"
                "== [[Wikipédia:RAW/2000-01-01|RAW 2000-01-01]] ==\n"
                "{{RAW/Distribution|2000-01-01}} ~~~~");
  string stateJSON = cbl::readFile(m_stateFile.path());
  json::Value state = json::parse(stateJSON);
  CBL_ASSERT_EQ(state["rcContinueToken"].str(), "token:2000-01-01T10:00:01Z");
  CBL_ASSERT_EQ(state["lastissue"].str(), "Wikipédia:RAW/2000-01-01");

  CBL_ASSERT(m_rawDistributor->run("", "", "", false, false));
}

void RAWDistributorTest::testRemoveOldMessages() {
  m_wiki.setPageContent("Wikipédia:RAW/2000-01-01", m_goodIssueContent);
  m_wiki.setPageContent("Discussion utilisateur:TestUser1",
                        "Header 1\n\n"
                        "==[[Wikipédia:RAW/1999-12-01|RAW 1999-12-01]]==\n"
                        "{{RAW/Distribution|1999-12-01}} ~~~~\n"
                        "<!-- Message envoyé par User:Test@frwiki en utilisant la liste à Test -->\n\n\n"
                        "== Test section ==\n\n"
                        "== [[Wikipédia:RAW/1999-12-15|RAW 1999-12-15]] ==\n"
                        "{{RAW/Distribution|1999-12-15}} ~~~~");
  m_wiki.setPageContent("Discussion utilisateur:TestUser2",
                        "Header 2\n\n"
                        "== [[Wikipédia:RAW/1999-12-01|RAW 1999-12-01]] ==\n"
                        "{{RAW/Distribution|1999-12-01}} ~~~~\n\n"
                        "== Test section ==\n\n"
                        "== [[Wikipédia:RAW/1999-12-15|RAW 1999-12-15]] ==\n"
                        "{{RAW/Distribution|1999-12-15}} ~~~~");
  m_wiki.setPageContent(
      "Discussion utilisateur:TestUser3",
      "Header 3\n\n"
      "== [[Wikipédia:Regards sur l'actualité de la Wikimedia/2012/48|RAW 74]] ==\n"
      "{{Regards sur l'actualité de la Wikimedia/PdD|2012|48}}\n"
      "— [[user_talk:Cantons-de-l'Est|Cantons-de-l'Est]] 7 décembre 2012 à 15:16 (CET)\n"
      "== [[Wikipédia:RAW/2013-04-05|RAW 2013-04-05]] ==\n"
      "{{RAW/PdD|2013-04-05}}\n"
      "== [[Wikipédia:RAW/2013-05-05|RAW 2013-05-05]] ==\n"
      "{{RAW/Distribution|2013-05-05}}\n"
      "— [[user_talk:Cantons-de-l'Est|Cantons-de-l'Est]]\n"
      "[[Utilisateur:Cantons-de-l&#39;Est|Cantons-de-l&#39;Est]]\n"
      "— [[User:Cantons-de-l'Est|Cantons-de-l'Est]]\n"
      "— [[Utilisateur:Cantons-de-l'Est|Cantons-de-l'Est]]\n"
      "[[Utilisateur:BeBot|BeBot]] ([[Discussion utilisateur:BeBot|d]])\n"
      "-- [[user_talk:Cantons-de-l'Est|Cantons-de-l'Est]] 5 janvier 2012 à 14:52 (CET)\n"
      "== [[:w:fr:Wikipédia:RAW/2014-01-10|RAW 2014-01-10]] ==\n"
      "<table style=\"background-color:white; padding:0px; border:1px solid #AAAAAA; "
      "border-radius: 15px; margin:0 auto;\"><tr><td style=\"text-align: center; font-weight:900; "
      "font-size:150%; text-shadow:gray 0.1em 0.1em 0.1em;\">Regards sur l'actualité de la Wikimedia\n"
      "<!-- Message envoyé par User:Cantons-de-l'Est@frwiki -->\n"
      "== [[Wikipédia:RAW/1999-10-15|RAW 1999-10-15]] ==\n"
      "{{RAW/Distribution|1999-11-01}} ~~~~\n\n"
      "== [[Wikipédia:RAW/1999-11-01|RAW 1999-11-01]] ==\n"
      "{{RAW/Distribution|1999-11-01}} ~~~~\n\n"
      "== [[Wikipédia:RAW/1999-11-15|RAW 1999-11-15]] ==\n"
      "{{RAW/Distribution|1999-11-15}} ~~~~\n"
      ":Edit\n\n"
      "== [[Wikipédia:RAW/1999-12-01|RAW 1999-12-01]] ==\n"
      "{{RAW/Distribution|1999-12-01}} ~~~~\n\n"
      "== Test section ==\n\n"
      "== [[Wikipédia:RAW/1999-12-15|RAW 1999-12-15]] ==\n"
      "{{RAW/Distribution|1999-12-15}} ~~~~");
  m_recentChangesReader.addMove("Wikipédia:RAW/Rédaction", "Wikipédia:RAW/2000-01-01",
                                Date::fromISO8601("2000-01-01T10:00:00Z"), "TrustedUser");
  Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T10:02:00Z"));
  m_wiki.setPageContent("Wikipédia:RAW/Inscription",
                        "*{{#target:User:TestUser1|fr.wikipedia.org}}\n"
                        "*{{#target:User:TestUser2|fr.wikipedia.org}} {{Ne pas purger les anciens numéros}}\n"
                        "*{{ #target:Utilisateur:TestUser3 | fr.wikipedia.org }}");
  CBL_ASSERT(m_rawDistributor->run("", "", "", false, false));
  CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:TestUser1"),
                "Header 1\n\n"
                "== Test section ==\n\n"
                "== [[Wikipédia:RAW/1999-12-15|RAW 1999-12-15]] ==\n"
                "{{RAW/Distribution|1999-12-15}} ~~~~\n\n"
                "== [[Wikipédia:RAW/2000-01-01|RAW 2000-01-01]] ==\n"
                "{{RAW/Distribution|2000-01-01}} ~~~~");
  CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:TestUser2"),
                "Header 2\n\n"
                "== [[Wikipédia:RAW/1999-12-01|RAW 1999-12-01]] ==\n"
                "{{RAW/Distribution|1999-12-01}} ~~~~\n\n"
                "== Test section ==\n\n"
                "== [[Wikipédia:RAW/1999-12-15|RAW 1999-12-15]] ==\n"
                "{{RAW/Distribution|1999-12-15}} ~~~~\n\n"
                "== [[Wikipédia:RAW/2000-01-01|RAW 2000-01-01]] ==\n"
                "{{RAW/Distribution|2000-01-01}} ~~~~");
  CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:TestUser3"),
                "Header 3\n\n"
                "== [[Wikipédia:RAW/1999-11-15|RAW 1999-11-15]] ==\n"
                "{{RAW/Distribution|1999-11-15}} ~~~~\n"
                ":Edit\n\n"
                "== Test section ==\n\n"
                "== [[Wikipédia:RAW/1999-12-15|RAW 1999-12-15]] ==\n"
                "{{RAW/Distribution|1999-12-15}} ~~~~\n\n"
                "== [[Wikipédia:RAW/2000-01-01|RAW 2000-01-01]] ==\n"
                "{{RAW/Distribution|2000-01-01}} ~~~~");
}

void RAWDistributorTest::testMoveFiltering() {
  json::Value state;
  state.getMutable("lastissue") = "Wikipédia:RAW/1999-12-30";
  reset(state.toJSON());
  Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T09:57:00Z"));
  CBL_ASSERT(m_rawDistributor->run("", "", "", false, false));
  m_wiki.setPageContent("Wikipédia:RAW/2000-01-01", m_goodIssueContent);
  m_wiki.setPageContent("Wikipédia:RAW/2000-02-01", m_goodIssueContent);
  m_wiki.setPageContent("Wikipédia:RAW/1999-12-01", m_goodIssueContent);
  m_wiki.setPageContent("Wikipédia:RAW/1999-12-30", m_goodIssueContent);
  m_wiki.setPageContent("Wikipédia:RAW/1999-12-32", m_goodIssueContent);
  m_wiki.setPageContent("Wikipédia:RAW/1999-12-31", m_goodIssueContent);
  m_wiki.setPageContent("Discussion utilisateur:TestUser1", ".");

  // Non-authorized user.
  m_recentChangesReader.addMove("Wikipédia:RAW/Rédaction", "Wikipédia:RAW/2000-01-01",
                                Date::fromISO8601("2000-01-01T10:00:00Z"), "UntrustedUser");
  CBL_ASSERT(!m_rawDistributor->run("", "", "", false, false));
  CBL_ASSERT_EQ(m_wiki.readPageContent("Utilisateur:OrlodrimBot/RAW/Erreur"),
                "{{u'|UntrustedUser}} n'est pas autopatrolled.");

  // Issue too far in the past or too far in the future.
  m_recentChangesReader.addMove("Wikipédia:RAW/Rédaction", "Wikipédia:RAW/2000-02-01",
                                Date::fromISO8601("2000-01-01T10:00:01Z"), "TrustedUser");
  m_recentChangesReader.addMove("Wikipédia:RAW/Rédaction", "Wikipédia:RAW/1999-12-01",
                                Date::fromISO8601("2000-01-01T10:00:02Z"), "TrustedUser");
  // Bad titles.
  m_recentChangesReader.addMove("Wikipédia:RAW/Rédaction", "Wikipédia:RAW/1999-12-31b",
                                Date::fromISO8601("2000-01-01T10:00:03Z"), "TrustedUser");
  m_recentChangesReader.addMove("Wikipédia:RAW/Rédaction", "Wikipédia:RAW/1999-13-01",
                                Date::fromISO8601("2000-01-01T10:00:04Z"), "TrustedUser");
  CBL_ASSERT(m_rawDistributor->run("", "", "", false, false));
  CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:TestUser1"), ".");

  // Issue already distributed.
  m_recentChangesReader.addMove("Wikipédia:RAW/Rédaction", "Wikipédia:RAW/1999-12-30",
                                Date::fromISO8601("2000-01-01T10:00:05Z"), "TrustedUser");
  CBL_ASSERT(!m_rawDistributor->run("", "", "", false, false));
  CBL_ASSERT_EQ(m_wiki.readPageContent("Utilisateur:OrlodrimBot/RAW/Erreur"),
                "La distribution de [[Wikipédia:RAW/1999-12-30]] a échoué : ce numéro a déjà été distribué.");

  // Valid publication (even if the source title is non-standard).
  m_recentChangesReader.addMove("Wikipédia:RAW/1999-13-01", "Wikipédia:RAW/1999-12-31",
                                Date::fromISO8601("2000-01-01T10:00:06Z"), "TrustedUser");
  Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T10:02:00Z"));
  m_wiki.setPageContent("Wikipédia:RAW/Inscription", "*{{#target:User:TestUser1|fr.wikipedia.org}}");
  CBL_ASSERT(m_rawDistributor->run("", "", "", false, false));
  CBL_ASSERT(m_wiki.readPageContent("Discussion utilisateur:TestUser1").find("RAW 1999-12-31") != string::npos);
}

void RAWDistributorTest::testNewsletterContentFiltering() {
  m_wiki.setPageContent("Discussion utilisateur:TestUser1", ".");
  m_wiki.setPageContent("Wikipédia:RAW/Inscription", "*{{#target:User:TestUser1|fr.wikipedia.org}}");

  m_recentChangesReader.addMove("A", "Wikipédia:RAW/2000-01-01", Date::fromISO8601("2000-01-01T10:00:00Z"),
                                "TrustedUser");
  Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T10:02:00Z"));
  CBL_ASSERT(!m_rawDistributor->run("", "", "", false, false));
  CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:TestUser1"), ".");
  CBL_ASSERT_EQ(m_wiki.readPageContent("Utilisateur:OrlodrimBot/RAW/Erreur"),
                "La distribution de [[Wikipédia:RAW/2000-01-01]] a échoué : la page n'existe pas.");

  m_recentChangesReader.addMove("A", "Wikipédia:RAW/2000-01-01", Date::fromISO8601("2000-01-01T10:05:00Z"),
                                "TrustedUser");
  Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T10:07:00Z"));
  m_wiki.setPageContent("Wikipédia:RAW/2000-01-01", "#REDIRECTION [[Wikipédia:RAW/Rédaction]]");
  CBL_ASSERT(!m_rawDistributor->run("", "", "", false, false));
  CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:TestUser1"), ".");
  CBL_ASSERT_EQ(m_wiki.readPageContent("Utilisateur:OrlodrimBot/RAW/Erreur"),
                "La distribution de [[Wikipédia:RAW/2000-01-01]] a échoué : la page est trop courte.");

  m_recentChangesReader.addMove("A", "Wikipédia:RAW/2000-01-01", Date::fromISO8601("2000-01-01T10:10:00Z"),
                                "TrustedUser");
  Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T10:12:00Z"));
  m_wiki.setPageContent("Wikipédia:RAW/2000-01-01", m_goodIssueContent);
  CBL_ASSERT(m_rawDistributor->run("", "", "", false, false));
  CBL_ASSERT(m_wiki.readPageContent("Discussion utilisateur:TestUser1") != ".");
}

void RAWDistributorTest::testTargetPageFiltering() {
  m_wiki.setPageContent("Discussion utilisateur:TestUser2", "#REDIRECT [[Discussion utilisateur:TestUser2b]]");
  m_wiki.setPageContent("Discussion utilisateur:TestUser2b", ".");
  m_wiki.setPageContent("Discussion utilisateur:TestUser3", ".");
  m_wiki.setPageContent("Discussion utilisateur:TestUser4", "== [[Wikipédia:RAW/2000-01-01|RAW 2000-01-01]] ==");
  m_wiki.setPageContent("Discussion utilisateur:TestUser5", "#REDIRECT [[Article]]");
  m_wiki.setPageContent("Discussion utilisateur:TestUser6", "#REDIRECT [[Discussion utilisateur:TestUser6]]");
  m_wiki.setPageContent("Article", ".");
  m_wiki.setPageContent("Wikipédia:RAW/Inscription",
                        "*{{#target:User:TestUser1|fr.wikipedia.org}}\n"
                        "*{{#target:User:TestUser2|fr.wikipedia.org}}\n"
                        "*{{#target:User:TestUser3|fr.wikipedia.org}}\n"
                        "*{{#target:User:TestUser4|fr.wikipedia.org}}\n"
                        "*{{#target:User:TestUser5|fr.wikipedia.org}}\n"
                        "*{{#target:User:TestUser6|fr.wikipedia.org}}\n"
                        "*{{#target:User:TestUser7-WithContribs|fr.wikipedia.org}}");

  m_wiki.setPageContent("Wikipédia:RAW/2000-01-01", m_goodIssueContent);
  m_recentChangesReader.addMove("A", "Wikipédia:RAW/2000-01-01", Date::fromISO8601("2000-01-01T10:00:00Z"),
                                "TrustedUser");
  Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T10:02:00Z"));
  CBL_ASSERT(m_rawDistributor->run("", "", "", false, false));
  CBL_ASSERT(!m_wiki.pageExists("Discussion utilisateur:TestUser1"));
  CBL_ASSERT(m_wiki.readPageContent("Discussion utilisateur:TestUser2").find("RAW") == string::npos);
  CBL_ASSERT(m_wiki.readPageContent("Discussion utilisateur:TestUser2b").find("RAW") != string::npos);
  CBL_ASSERT(m_wiki.readPageContent("Discussion utilisateur:TestUser2b").find("redirige ici") != string::npos);
  CBL_ASSERT(m_wiki.readPageContent("Discussion utilisateur:TestUser3").find("RAW") != string::npos);
  CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:TestUser4"),
                "== [[Wikipédia:RAW/2000-01-01|RAW 2000-01-01]] ==");
  CBL_ASSERT(m_wiki.readPageContent("Discussion utilisateur:TestUser5").find("RAW") == string::npos);
  CBL_ASSERT(m_wiki.readPageContent("Article").find("RAW") == string::npos);
  CBL_ASSERT(m_wiki.readPageContent("Discussion utilisateur:TestUser6").find("RAW") == string::npos);
  CBL_ASSERT(m_wiki.readPageContent("Discussion utilisateur:TestUser7-WithContribs").find("RAW") != string::npos);
}

void RAWDistributorTest::testFlow() {
  m_wiki.setPageContent("Discussion utilisateur:FlowUser", ".");
  m_wiki.setPageContent("Discussion utilisateur:User2", "#REDIRECT [[Discussion utilisateur:FlowUser2]]");
  m_wiki.setPageContent("Discussion utilisateur:FlowUser2", ".");
  m_wiki.setPageContent("Wikipédia:RAW/Inscription",
                        "*{{#target:User:FlowUser|fr.wikipedia.org}}\n"
                        "*{{#target:User:User2|fr.wikipedia.org}}");
  m_wiki.setPageContent("Wikipédia:RAW/2000-01-01", m_goodIssueContent);
  m_recentChangesReader.addMove("A", "Wikipédia:RAW/2000-01-01", Date::fromISO8601("2000-01-01T10:00:00Z"),
                                "TrustedUser");
  Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T10:02:00Z"));
  CBL_ASSERT(m_rawDistributor->run("", "", "", false, false));
  CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:FlowUser"),
                ".\nFLOW|RAW 2000-01-01|{{RAW/Distribution|2000-01-01}}");
  CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:User2"),
                "#REDIRECT [[Discussion utilisateur:FlowUser2]]");
  CBL_ASSERT_EQ(
      m_wiki.readPageContent("Discussion utilisateur:FlowUser2"),
      ".\nFLOW|RAW 2000-01-01|{{RAW/Distribution|2000-01-01}}\n\n<small>Ce message vous est adressé car {{u'|User2}} "
      "est abonné à Regards sur l'actualité de la Wikimedia et [[Discussion utilisateur:User2]] redirige ici. Si vous "
      "avez renommé votre compte, pensez à mettre à jour votre nom dans la "
      "[[Wikipédia:RAW/Inscription|liste des abonnés]] pour ne plus voir cet avertissement. À l'inverse, si cette "
      "redirection est une erreur, [[Special:EditPage/Discussion utilisateur:User2|supprimez-la]] pour que les "
      "messages ne soient plus transmis.</small>");
}

}  // namespace

int main() {
  RAWDistributorTest rawDistributorTest;
  rawDistributorTest.run();
  return 0;
}
