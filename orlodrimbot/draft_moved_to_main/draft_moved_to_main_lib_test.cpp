#include "draft_moved_to_main_lib.h"
#include <string>
#include <vector>
#include "cbl/date.h"
#include "cbl/file.h"
#include "cbl/log.h"
#include "cbl/tempfile.h"
#include "cbl/unittest.h"
#include "mwclient/mock_wiki.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/mock_recent_changes_reader.h"

using cbl::Date;
using cbl::TempFile;
using mwc::UserInfo;
using std::string;
using std::vector;

class MoveToMainMockWiki : public mwc::MockWiki {
public:
  void getUsersInfo(int properties, vector<UserInfo>& users) override {
    CBL_ASSERT_EQ(properties, mwc::UIP_GROUPS);
    for (UserInfo& user : users) {
      user.groups = user.name.find("Trusted") == string::npos ? 0 : mwc::UG_AUTOPATROLLED;
    }
  }
};

class ListOfPublishedDraftsTest : public cbl::Test {
private:
  void setUp() override {
    m_wiki.resetDatabase();
    m_recentChangesReader.resetMock();
    cbl::writeFile(m_stateFile.path(), "{}");
  }

  CBL_TEST_CASE(eventsFiltering) {
    // Ignored (too old).
    m_recentChangesReader.addMove("1999-12-31T11:00:00Z", "Utilisateur:U1/Draft", "Article 1", "U1");
    // Ignored (source is in the main namespace).
    m_recentChangesReader.addMove("2000-01-01T04:00:00Z", "Article 2 old name", "Article 2", "U2");
    // Added to the list (we detect moves from any namespace != main, because new users often move their draft to
    // a bad namespace like "Wikipédia:" before finding the right way to publish it).
    m_recentChangesReader.addMove("2000-01-01T08:00:00Z", "Catégorie:Some category", "Article 3", "U3");
    // Ignored (target is not in the main namespace).
    m_recentChangesReader.addMove("2000-01-01T12:00:00Z", "Utilisateur:U4/Draft", "Utilisateur:U4/Draft2", "U4");
    // Added to the list (standard draft publication).
    m_recentChangesReader.addMove("2000-01-01T16:00:00Z", "Utilisateur:U5/Draft", "Article 5", "U5");
    // Ignored (user is autopatrolled).
    m_recentChangesReader.addMove("2000-01-01T20:00:00Z", "Utilisateur:U6/Draft", "Article 6", "Trusted U6");
    // Added to the list.
    m_recentChangesReader.addMove("2000-01-02T00:00:00Z", "Utilisateur:U7/Draft", "Article 7", "U7");

    Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-02T00:00:00Z"));
    ListOfPublishedDrafts listOfPublishedDrafts(&m_wiki, &m_recentChangesReader, m_stateFile.path(), 2);
    listOfPublishedDrafts.update(/* dryRun = */ false);
    CBL_ASSERT_EQ(m_wiki.readPageContent(LIST_TITLE),
                  "<!-- BEGIN BOT SECTION -->\n"
                  "== 2 janvier 2000 ==\n"
                  "*2 janvier 2000 à 00:00 {{u|U7}} a déplacé la page [[Utilisateur:U7/Draft]] vers [[Article 7]]\n\n"
                  "== {{1er}} janvier 2000 ==\n"
                  "*1 janvier 2000 à 16:00 {{u|U5}} a déplacé la page [[Utilisateur:U5/Draft]] vers [[Article 5]]\n"
                  "*1 janvier 2000 à 08:00 {{u|U3}} a déplacé la page [[:Catégorie:Some category]] vers [[Article 3]]\n"
                  "<!-- END BOT SECTION -->");
    m_wiki.assertPageLastCommentEquals(LIST_TITLE, "[[Article 3]], [[Article 5]], [[Article 7]]");
  }

  CBL_TEST_CASE(ignoreDeletedArticles) {
    m_recentChangesReader.addMove("2000-01-01T12:00:00Z", "Utilisateur:U1/Draft", "Article 1", "U1");
    m_recentChangesReader.addMove("2000-01-01T12:01:00Z", "Utilisateur:U2/Draft", "Article 2", "U2");
    m_recentChangesReader.addDeletion("2000-01-01T12:05:00Z", "Article 1");

    Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-02T00:00:00Z"));
    ListOfPublishedDrafts listOfPublishedDrafts(&m_wiki, &m_recentChangesReader, m_stateFile.path(), 2);
    listOfPublishedDrafts.update(/* dryRun = */ false);
    CBL_ASSERT_EQ(m_wiki.readPageContent(LIST_TITLE),
                  "<!-- BEGIN BOT SECTION -->\n"
                  "== {{1er}} janvier 2000 ==\n"
                  "*1 janvier 2000 à 12:01 {{u|U2}} a déplacé la page [[Utilisateur:U2/Draft]] vers [[Article 2]]\n"
                  "<!-- END BOT SECTION -->");
  }

  CBL_TEST_CASE(followMoves) {
    // Article published and then renamed.
    m_recentChangesReader.addMove("2000-01-01T12:00:00Z", "Utilisateur:U1/Draft", "Article 1", "U1");
    m_recentChangesReader.addMove("2000-01-01T12:05:00Z", "Article 1", "Article 1b");

    // Article published and then renamed. The second move overwrites an existing page.
    m_recentChangesReader.addMove("2000-01-01T12:10:00Z", "Utilisateur:U2/Draft", "Article 2", "U2");
    m_recentChangesReader.addMove("2000-01-01T12:15:00Z", "Article 2", "Article 2b");
    m_recentChangesReader.addDeletion("2000-01-01T12:15:00Z", "Article 2b");

    // Article published and moved back to the user namespace.
    m_recentChangesReader.addMove("2000-01-01T12:20:00Z", "Utilisateur:U3/Draft", "Article 3", "U3");
    m_recentChangesReader.addMove("2000-01-01T12:25:00Z", "Article 3", "Utilisateur:U3/Draft");
    m_recentChangesReader.lastRC().mutableLogEvent().action = "move_redir";

    // Article published and moved back to the user namespace. A redirect is unexpectedly left.
    m_recentChangesReader.addMove("2000-01-01T12:30:00Z", "Utilisateur:U4/Draft", "Article 4", "U4");
    m_recentChangesReader.addMove("2000-01-01T12:35:00Z", "Article 4", "Utilisateur:U4/Draft");

    // Article published and moved back to the user namespace. A redirect is left but deleted afterwards.
    m_recentChangesReader.addMove("2000-01-01T12:40:00Z", "Utilisateur:U5/Draft", "Article 5", "U5");
    m_recentChangesReader.addMove("2000-01-01T12:45:00Z", "Article 5", "Utilisateur:U5/Draft");
    m_recentChangesReader.addDeletion("2000-01-01T12:50:00Z", "Article 5");

    Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-02T00:00:00Z"));
    ListOfPublishedDrafts listOfPublishedDrafts(&m_wiki, &m_recentChangesReader, m_stateFile.path(), 2);
    listOfPublishedDrafts.update(/* dryRun = */ false);
    CBL_ASSERT_EQ(m_wiki.readPageContent(LIST_TITLE),
                  "<!-- BEGIN BOT SECTION -->\n"
                  "== {{1er}} janvier 2000 ==\n"
                  "*1 janvier 2000 à 12:30 {{u|U4}} a déplacé la page [[Utilisateur:U4/Draft]] vers [[Article 4]]\n"
                  "*1 janvier 2000 à 12:10 {{u|U2}} a déplacé la page [[Utilisateur:U2/Draft]] vers [[Article 2]] "
                  "(titre actuel : [[Article 2b]])\n"
                  "*1 janvier 2000 à 12:00 {{u|U1}} a déplacé la page [[Utilisateur:U1/Draft]] vers [[Article 1]] "
                  "(titre actuel : [[Article 1b]])\n"
                  "<!-- END BOT SECTION -->");
  }

  CBL_TEST_CASE(multipleUpdates) {
    m_recentChangesReader.addMove("2000-01-01T08:00:00Z", "Utilisateur:U1/Draft", "Article 1", "U1");
    m_recentChangesReader.addMove("2000-01-02T00:00:00Z", "Utilisateur:U2/Draft", "Article 2", "U2");

    Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-02T00:00:00Z"));
    ListOfPublishedDrafts listOfPublishedDrafts(&m_wiki, &m_recentChangesReader, m_stateFile.path(), 2);
    listOfPublishedDrafts.update(/* dryRun = */ false);
    CBL_ASSERT_EQ(m_wiki.readPageContent(LIST_TITLE),
                  "<!-- BEGIN BOT SECTION -->\n"
                  "== 2 janvier 2000 ==\n"
                  "*2 janvier 2000 à 00:00 {{u|U2}} a déplacé la page [[Utilisateur:U2/Draft]] vers [[Article 2]]\n\n"
                  "== {{1er}} janvier 2000 ==\n"
                  "*1 janvier 2000 à 08:00 {{u|U1}} a déplacé la page [[Utilisateur:U1/Draft]] vers [[Article 1]]\n"
                  "<!-- END BOT SECTION -->");
    m_wiki.assertPageLastCommentEquals(LIST_TITLE, "[[Article 1]], [[Article 2]]");

    m_recentChangesReader.addMove("2000-01-02T10:00:00Z", "Utilisateur:U3/Draft", "Article 3", "U3");
    m_recentChangesReader.addMove("2000-01-02T20:00:00Z", "Utilisateur:U4/Draft", "Article 4", "U4");

    Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-02T23:00:00Z"));
    listOfPublishedDrafts.update(/* dryRun = */ false);
    CBL_ASSERT_EQ(m_wiki.readPageContent(LIST_TITLE),
                  "<!-- BEGIN BOT SECTION -->\n"
                  "== 2 janvier 2000 ==\n"
                  "*2 janvier 2000 à 20:00 {{u|U4}} a déplacé la page [[Utilisateur:U4/Draft]] vers [[Article 4]]\n"
                  "*2 janvier 2000 à 10:00 {{u|U3}} a déplacé la page [[Utilisateur:U3/Draft]] vers [[Article 3]]\n"
                  "*2 janvier 2000 à 00:00 {{u|U2}} a déplacé la page [[Utilisateur:U2/Draft]] vers [[Article 2]]\n\n"
                  "== {{1er}} janvier 2000 ==\n"
                  "*1 janvier 2000 à 08:00 {{u|U1}} a déplacé la page [[Utilisateur:U1/Draft]] vers [[Article 1]]\n"
                  "<!-- END BOT SECTION -->");
    m_wiki.assertPageLastCommentEquals(LIST_TITLE, "[[Article 3]], [[Article 4]]");

    m_recentChangesReader.addMove("2000-01-03T12:00:00Z", "Utilisateur:U5/Draft", "Article 5", "U5");
    m_recentChangesReader.addMove("2000-01-04T00:30:00Z", "Utilisateur:U6/Draft", "Article 6", "U6");

    Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-04T01:00:00Z"));
    listOfPublishedDrafts.update(/* dryRun = */ false);
    CBL_ASSERT_EQ(m_wiki.readPageContent(LIST_TITLE),
                  "<!-- BEGIN BOT SECTION -->\n"
                  "== 4 janvier 2000 ==\n"
                  "*4 janvier 2000 à 00:30 {{u|U6}} a déplacé la page [[Utilisateur:U6/Draft]] vers [[Article 6]]\n\n"
                  "== 3 janvier 2000 ==\n"
                  "*3 janvier 2000 à 12:00 {{u|U5}} a déplacé la page [[Utilisateur:U5/Draft]] vers [[Article 5]]\n\n"
                  "== 2 janvier 2000 ==\n"
                  "*2 janvier 2000 à 20:00 {{u|U4}} a déplacé la page [[Utilisateur:U4/Draft]] vers [[Article 4]]\n"
                  "*2 janvier 2000 à 10:00 {{u|U3}} a déplacé la page [[Utilisateur:U3/Draft]] vers [[Article 3]]\n"
                  "*2 janvier 2000 à 00:00 {{u|U2}} a déplacé la page [[Utilisateur:U2/Draft]] vers [[Article 2]]\n"
                  "<!-- END BOT SECTION -->");
  }

  CBL_TEST_CASE(longComment) {
    for (int i = 1; i <= 100; i++) {
      m_recentChangesReader.addMove("2000-01-01T12:00:00Z", "Utilisateur:U1/Draft", "Article " + std::to_string(i),
                                    "U1");
    }
    Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-02T00:00:00Z"));
    ListOfPublishedDrafts listOfPublishedDrafts(&m_wiki, &m_recentChangesReader, m_stateFile.path(), 2);
    listOfPublishedDrafts.update(/* dryRun = */ false);
    m_wiki.assertPageLastCommentEquals(
        LIST_TITLE,
        "[[Article 1]], [[Article 2]], [[Article 3]], [[Article 4]], [[Article 5]], [[Article 6]], [[Article 7]], "
        "[[Article 8]], [[Article 9]], [[Article 10]], [[Article 11]], [[Article 12]], [[Article 13]], [[Article 14]], "
        "[[Article 15]], [[Article 16]], [[Article 17]], [[Article 18]], [[Article 19]], [[Article 20]], "
        "[[Article 21]], [[Article 22]], [[Article 23]], [[Article 24]], [[Article 25]], 75 autres pages");
  }

  MoveToMainMockWiki m_wiki;
  live_replication::MockRecentChangesReader m_recentChangesReader;
  TempFile m_stateFile;
};

int main() {
  ListOfPublishedDraftsTest().run();
  return 0;
}
