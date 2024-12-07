#include "bot_requests_archiver_lib.h"
#include <string_view>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/unittest.h"
#include "mwclient/mock_wiki.h"

using cbl::Date;
using std::string_view;

constexpr const char* MAIN_PAGE_OLD_CONTENT =
    "En-tête ligne 1\n"
    "En-tête ligne 2\n"
    "== Requête 1 ==\n"
    "{{#ifeq:{{#titleparts:{{PAGENAME}}|1|-3}}|Archives||{{#ifexpr:{{#time:U}}<1603881778\n"
    " |<small>''Cette requête sera archivée [[Wikipédia:Bot/Requêtes/Archives/2020/10|ici]] à partir du '''28 octobre "
    "2020'''.''</small>\n"
    " |<span style=\"color:red\">Depuis le 28 octobre 2020, merci d'archiver cette requête </span> "
    "[[Wikipédia:Bot/Requêtes/Archives/2020/10|→ ici ←]]}}}}\n"
    "== Requête 2 ==\n"
    "{{#ifeq:{{#titleparts:{{PAGENAME}}|1|-3}}|Archives||{{#ifexpr:{{#time:U}}<1603881778\n"
    " |<small>''Cette requête sera archivée [[Wikipédia:Bot/Requêtes/Archives/2020/10|ici]] à partir du '''30 octobre "
    "2020'''.''</small>\n"
    " |<span style=\"color:red\">Depuis le 30 octobre 2020, merci d'archiver cette requête </span> "
    "[[Wikipédia:Bot/Requêtes/Archives/2020/10|→ ici ←]]}}}}";

constexpr string_view MAIN_PAGE_NEW_CONTENT =
    "En-tête ligne 1\n"
    "En-tête ligne 2\n"
    "== Requête 2 ==\n"
    "{{#ifeq:{{#titleparts:{{PAGENAME}}|1|-3}}|Archives||{{#ifexpr:{{#time:U}}<1603881778\n"
    " |<small>''Cette requête sera archivée [[Wikipédia:Bot/Requêtes/Archives/2020/10|ici]] à partir du '''30 octobre "
    "2020'''.''</small>\n"
    " |<span style=\"color:red\">Depuis le 30 octobre 2020, merci d'archiver cette requête </span> "
    "[[Wikipédia:Bot/Requêtes/Archives/2020/10|→ ici ←]]}}}}";

constexpr string_view ARCHIVE_PAGE_NEW_CONTENT =
    "<noinclude>{{Wikipédia:Bot/Navig}}</noinclude>\n\n"
    "== Requête 1 ==\n"
    "{{#ifeq:{{#titleparts:{{PAGENAME}}|1|-3}}|Archives||{{#ifexpr:{{#time:U}}<1603881778\n"
    " |<small>''Cette requête sera archivée [[Wikipédia:Bot/Requêtes/Archives/2020/10|ici]] à partir du '''28 octobre "
    "2020'''.''</small>\n"
    " |<span style=\"color:red\">Depuis le 28 octobre 2020, merci d'archiver cette requête </span> "
    "[[Wikipédia:Bot/Requêtes/Archives/2020/10|→ ici ←]]}}}}";

constexpr string_view ARCHIVE_PAGE_NEW_CONTENT_2 =
    "Archive\n\n"
    "== Requête 1 ==\n"
    "{{#ifeq:{{#titleparts:{{PAGENAME}}|1|-3}}|Archives||{{#ifexpr:{{#time:U}}<1603881778\n"
    " |<small>''Cette requête sera archivée [[Wikipédia:Bot/Requêtes/Archives/2020/10|ici]] à partir du '''28 octobre "
    "2020'''.''</small>\n"
    " |<span style=\"color:red\">Depuis le 28 octobre 2020, merci d'archiver cette requête </span> "
    "[[Wikipédia:Bot/Requêtes/Archives/2020/10|→ ici ←]]}}}}";

class BotRequestsArchiverTest : public cbl::Test {
public:
  BotRequestsArchiverTest() : m_archiver(m_wiki, /* dryRun = */ false) {}

private:
  void setUp() override { m_wiki.resetDatabase(); }

  CBL_TEST_CASE(CreateOrAppendToArchive) {
    Date::setFrozenValueOfNow(Date::fromISO8601("2020-10-28T12:00:00Z"));

    m_wiki.setPageContent("Wikipédia:Bot/Requêtes/2019/12", MAIN_PAGE_OLD_CONTENT);
    m_wiki.setPageContent("Wikipédia:Bot/Requêtes/2020/08", ".");
    m_wiki.setPageContent("Wikipédia:Bot/Requêtes/2020/09", ".");
    m_wiki.setPageContent("Wikipédia:Bot/Requêtes/2020/10", MAIN_PAGE_OLD_CONTENT);
    m_wiki.setPageContent("Wikipédia:Bot/Requêtes/Archives/2020/10", "Archive");

    m_archiver.run(/* forceNewMonth = */ false);

    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Bot/Requêtes/2019/12"), MAIN_PAGE_NEW_CONTENT);
    m_wiki.assertPageLastCommentEquals("Wikipédia:Bot/Requêtes/2019/12",
                                       "Archivage d'une requête vers [[Wikipédia:Bot/Requêtes/Archives/2019/12]]");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Bot/Requêtes/Archives/2019/12"), ARCHIVE_PAGE_NEW_CONTENT);
    m_wiki.assertPageLastCommentEquals("Wikipédia:Bot/Requêtes/Archives/2019/12",
                                       "Archivage d'une requête depuis [[Wikipédia:Bot/Requêtes/2019/12]]");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Bot/Requêtes/2020/08"), ".");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Bot/Requêtes/2020/09"), ".");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Bot/Requêtes/2020/10"), MAIN_PAGE_NEW_CONTENT);
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Bot/Requêtes/Archives/2020/10"), ARCHIVE_PAGE_NEW_CONTENT_2);
    CBL_ASSERT_EQ(m_wiki.getNumPages(), 6);
  }

  CBL_TEST_CASE(InitializePageForNextMonth) {
    Date::setFrozenValueOfNow(Date::fromISO8601("2018-12-31T23:00:00Z"));
    m_archiver.run(/* forceNewMonth = */ false);
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Bot/Requêtes/2019/01"),
                  "<noinclude>{{Wikipédia:Bot/Navig}}</noinclude>");
    m_wiki.assertPageLastCommentEquals("Wikipédia:Bot/Requêtes/2019/01", "Initialisation");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Bot/Requêtes/Archives/2019/01"),
                  "<noinclude>{{Wikipédia:Bot/Navig}}</noinclude>");
    m_wiki.assertPageLastCommentEquals("Wikipédia:Bot/Requêtes/Archives/2019/01", "Initialisation");
    CBL_ASSERT_EQ(m_wiki.getNumPages(), 2);
  }

  CBL_TEST_CASE(ArchiveAllRequestsAfterAYear) {
    m_wiki.resetDatabase();
    Date::setFrozenValueOfNow(Date::fromISO8601("2018-12-31T23:00:00Z"));
    m_wiki.setPageContent("Wikipédia:Bot/Requêtes/2017/12", "En-tête\n==2==\n==3==");
    m_wiki.setPageContent("Wikipédia:Bot/Requêtes/Archives/2017/12", "Archives\n==1==");
    m_wiki.setPageContent("Wikipédia:Bot/Requêtes/2018/01", "En-tête\n==2==\n==3==");
    m_archiver.run(/* forceNewMonth = */ false);
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Bot/Requêtes/2017/12"),
                  "#REDIRECTION [[Wikipédia:Bot/Requêtes/Archives/2017/12]]");
    m_wiki.assertPageLastCommentEquals("Wikipédia:Bot/Requêtes/2017/12",
                                       "Archivage de 2 requêtes et transformation en redirection vers la page "
                                       "d'archives [[Wikipédia:Bot/Requêtes/Archives/2017/12]]");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Bot/Requêtes/Archives/2017/12"), "Archives\n==1==\n\n==2==\n==3==");
    m_wiki.assertPageLastCommentEquals("Wikipédia:Bot/Requêtes/Archives/2017/12",
                                       "Archivage de 2 requêtes depuis [[Wikipédia:Bot/Requêtes/2017/12]]");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Bot/Requêtes/2018/01"), "En-tête\n==2==\n==3==");
  }

  // When the page of the current month becomes empty, do not immediately redirect it, but do so at the end of the
  // month.
  CBL_TEST_CASE(DoNotRedirectPageOfCurrentMonth) {
    m_wiki.resetDatabase();
    Date::setFrozenValueOfNow(Date::fromISO8601("2020-12-20T23:00:00Z"));
    m_wiki.setPageContent("Wikipédia:Bot/Requêtes/2020/11", MAIN_PAGE_OLD_CONTENT);
    m_wiki.setPageContent("Wikipédia:Bot/Requêtes/2020/12", MAIN_PAGE_OLD_CONTENT);
    m_archiver.run(/* forceNewMonth = */ false);
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Bot/Requêtes/2020/11"),
                  "#REDIRECTION [[Wikipédia:Bot/Requêtes/Archives/2020/11]]");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Bot/Requêtes/2020/12"), "En-tête ligne 1\nEn-tête ligne 2");
    Date::setFrozenValueOfNow(Date::fromISO8601("2020-12-31T23:00:00Z"));
    m_archiver.run(/* forceNewMonth = */ false);
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Bot/Requêtes/2020/12"),
                  "#REDIRECTION [[Wikipédia:Bot/Requêtes/Archives/2020/12]]");
    m_wiki.assertPageLastCommentEquals("Wikipédia:Bot/Requêtes/2020/12",
                                       "Page redirigée vers [[Wikipédia:Bot/Requêtes/Archives/2020/12]]");
  }

  mwc::MockWiki m_wiki;
  BotRequestsArchiver m_archiver;
};

int main() {
  BotRequestsArchiverTest().run();
  return 0;
}
