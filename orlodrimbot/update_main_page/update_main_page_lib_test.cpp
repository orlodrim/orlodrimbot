#include "update_main_page_lib.h"
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include "cbl/date.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "cbl/unittest.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/mock_recent_changes_reader.h"
#include "mock_wiki_with_parse.h"
#include "template_expansion_cache.h"

using cbl::Date;
using cbl::DateDiff;
using std::make_unique;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::unordered_map;

constexpr char TEST_SOURCE_PAGE[] = "Modèle:Accueil actualité";
constexpr char TEST_TARGET_PAGE[] = "Modèle:Accueil actualité/Copie sans modèles";
constexpr char INITIAL_TARGET_CONTENT[] = "<!-- BEGIN BOT SECTION --><!-- END BOT SECTION -->";

string wrapInBotSection(const string& content) {
  return cbl::concat("<!-- BEGIN BOT SECTION -->\n", content, "\n<!-- END BOT SECTION -->");
}

unordered_map<string, string> parseQueryString(string_view query) {
  unordered_map<string, string> parameters;
  for (string_view field : cbl::split(query, '&')) {
    size_t equalPosition = field.find('=');
    parameters[string(field.substr(0, equalPosition))] = cbl::decodeURIComponent(field.substr(equalPosition + 1));
  }
  return parameters;
}

class UpdateMainPageTest : public cbl::Test {
private:
  void setUp() override {
    Date::setFrozenValueOfNow(Date::fromISO8601("2001-01-01T10:00:00Z"));
    m_wiki.resetDatabase();
    m_wiki.expandTemplatesCallCount = 0;
    m_wiki.setPageContent(TEST_SOURCE_PAGE, ".");
    m_wiki.setPageContent(TEST_TARGET_PAGE, INITIAL_TARGET_CONTENT);
    m_wiki.setPageContent("Wikipédia:Le saviez-vous ?/Anecdotes sur l'accueil/Copie sans modèles",
                          INITIAL_TARGET_CONTENT);
    m_wiki.setPageContent("Utilisateur:OrlodrimBot/Statut page d'accueil", "no reported errors");
    m_recentChangesReader.resetMock();
    m_state.setNull();
    m_state.getMutable("update_timestamp") = Date::now().toISO8601();
    m_state.getMutable("featured_articles_day") = Date(2001, 1, 1).toISO8601();
    m_templateExpansionCache = make_unique<TemplateExpansionCache>(&m_wiki, ":memory:");
    updateMainPage(m_wiki, m_state, m_recentChangesReader, *m_templateExpansionCache);
  }

  void updatePageContent(const string& title, const string& content) {
    m_wiki.setPageContent(title, content);
    mwc::RecentChange rc;
    rc.setType(mwc::RC_EDIT);
    mwc::Revision& revision = rc.mutableRevision();
    revision.timestamp = Date::now();
    revision.title = title;
    revision.revid = m_wiki.readPage(title, mwc::RP_REVID).revid;
    m_recentChangesReader.addRC(rc);
  }

  void callUpdateMainPage() { updateMainPage(m_wiki, m_state, m_recentChangesReader, *m_templateExpansionCache); }

  string getReportedErrors() { return m_wiki.readPageContent("Utilisateur:OrlodrimBot/Statut page d'accueil"); }
  void assertCopyFails(const string& errorSubstring) {
    string oldContent = m_wiki.readPageContent(TEST_TARGET_PAGE);
    callUpdateMainPage();
    CBL_ASSERT_EQ(m_wiki.readPageContent(TEST_TARGET_PAGE), oldContent);
    string errorMessage = getReportedErrors();
    CBL_ASSERT(errorMessage.find(errorSubstring) != string::npos) << errorMessage << " / " << errorSubstring;
  }

  CBL_TEST_CASE(StandardUpdates) {
    updatePageContent(TEST_SOURCE_PAGE, "Test content.");

    Date::advanceFrozenClock(DateDiff::fromMinutes(5));
    callUpdateMainPage();
    CBL_ASSERT_EQ(m_wiki.readPageContent(TEST_TARGET_PAGE), wrapInBotSection("Test content."));
    CBL_ASSERT_EQ(getReportedErrors(), "no reported errors");

    Date::advanceFrozenClock(DateDiff::fromMinutes(1));
    updatePageContent(TEST_SOURCE_PAGE, "Test content 2.");

    Date::advanceFrozenClock(DateDiff::fromMinutes(5));
    callUpdateMainPage();
    CBL_ASSERT_EQ(m_wiki.readPageContent(TEST_TARGET_PAGE), wrapInBotSection("Test content 2."));
  }
  CBL_TEST_CASE(RemoveNoinclude) {
    updatePageContent(TEST_SOURCE_PAGE, "Test content.<noinclude>{{Documentation}}</noinclude>");
    Date::advanceFrozenClock(DateDiff::fromMinutes(5));
    callUpdateMainPage();
    CBL_ASSERT_EQ(m_wiki.readPageContent(TEST_TARGET_PAGE), wrapInBotSection("Test content."));
  }
  CBL_TEST_CASE(Redirect) {
    updatePageContent(TEST_SOURCE_PAGE, "#REDIRECTION [[Other page]]");
    Date::advanceFrozenClock(DateDiff::fromMinutes(5));
    callUpdateMainPage();
    assertCopyFails("la page source est une redirection");
  }
  CBL_TEST_CASE(LongPage) {
    updatePageContent(TEST_SOURCE_PAGE, string(25'001, 'a'));
    Date::advanceFrozenClock(DateDiff::fromMinutes(5));
    callUpdateMainPage();
    assertCopyFails("la page source est trop longue (plus de 25 Ko)");
  }
  CBL_TEST_CASE(MissingBotSection) {
    m_wiki.setPageContent(TEST_TARGET_PAGE, ".");
    updatePageContent(TEST_SOURCE_PAGE, "Test content");
    Date::advanceFrozenClock(DateDiff::fromMinutes(5));
    assertCopyFails("section de bot non trouvée sur [[");
  }
  CBL_TEST_CASE(NoUpdateJustAfterEdit) {
    updatePageContent(TEST_SOURCE_PAGE, "Test content.");
    Date::advanceFrozenClock(DateDiff::fromMinutes(1));
    callUpdateMainPage();
    CBL_ASSERT_EQ(m_wiki.readPageContent(TEST_TARGET_PAGE), INITIAL_TARGET_CONTENT);
  }
  CBL_TEST_CASE(ExpandTemplate) {
    updatePageContent("Modèle:Abc", "Value");
    Date::advanceFrozenClock(DateDiff::fromMinutes(1));
    updatePageContent(TEST_SOURCE_PAGE, "{{abc}}");
    Date::advanceFrozenClock(DateDiff::fromMinutes(5));
    callUpdateMainPage();
    CBL_ASSERT_EQ(m_wiki.readPageContent(TEST_TARGET_PAGE), wrapInBotSection("{{expanded:abc}}"));
  }
  CBL_TEST_CASE(NoUpdateDueToRecentlyModifiedTemplate) {
    updatePageContent(TEST_SOURCE_PAGE, "{{abc}}");
    Date::advanceFrozenClock(DateDiff::fromMinutes(1));
    updatePageContent("Modèle:Abc", "Value");
    Date::advanceFrozenClock(DateDiff::fromMinutes(5));
    assertCopyFails("le modèle récemment modifié [[Modèle:Abc]] est inclus dans [[");
  }
  CBL_TEST_CASE(MultipleTemplates) {
    updatePageContent("Modèle:Abc", "Value1");
    Date::advanceFrozenClock(DateDiff::fromMinutes(1));
    updatePageContent(TEST_SOURCE_PAGE, "{{abc}} {{def}}");
    Date::advanceFrozenClock(DateDiff::fromMinutes(1));
    updatePageContent("Modèle:Def", "Value2");
    Date::advanceFrozenClock(DateDiff::fromMinutes(5));
    assertCopyFails("le modèle récemment modifié [[Modèle:Def]] est inclus dans [[");
  }
  CBL_TEST_CASE(SkipUpdateIfNoRecentChange) {
    m_wiki.setPageContent(TEST_SOURCE_PAGE, "Test content.");
    Date::advanceFrozenClock(DateDiff::fromMinutes(5));
    callUpdateMainPage();
    CBL_ASSERT_EQ(m_wiki.readPageContent(TEST_TARGET_PAGE), INITIAL_TARGET_CONTENT);
  }
  CBL_TEST_CASE(ProtectedStylesheets) {
    updatePageContent(TEST_SOURCE_PAGE,
                      "<templatestyles src=\"Test/styles.css\"></templatestyles>\n"
                      "<templatestyles src=\"Test2/styles.css\"></templatestyles>");
    m_wiki.setPageProtection("Modèle:Test/styles.css", {{.type = mwc::PRT_EDIT, .level = mwc::PRL_AUTOPATROLLED}});
    m_wiki.setPageProtection(
        "Modèle:Test2/styles.css",
        {{.type = mwc::PRT_EDIT, .level = mwc::PRL_SYSOP, .expiry = Date::now() + DateDiff::fromDays(10)}});
    Date::advanceFrozenClock(DateDiff::fromMinutes(5));
    callUpdateMainPage();
    CBL_ASSERT_EQ(m_wiki.readPageContent(TEST_TARGET_PAGE),
                  wrapInBotSection("<templatestyles src=\"Test/styles.css\"></templatestyles>\n"
                                   "<templatestyles src=\"Test2/styles.css\"></templatestyles>"));
  }
  CBL_TEST_CASE(UnprotectedStylesheet) {
    updatePageContent(TEST_SOURCE_PAGE, "<templatestyles src=\"Test/styles.css\"></templatestyles>");
    Date::advanceFrozenClock(DateDiff::fromMinutes(5));
    assertCopyFails("la feuille de style [[Modèle:Test/styles.css]] n'est pas protégée");
  }
  CBL_TEST_CASE(InsufficientlyProtectedStylesheet) {
    updatePageContent(TEST_SOURCE_PAGE, "<templatestyles src=\"Test/styles.css\"></templatestyles>");
    m_wiki.setPageProtection("Modèle:Test/styles.css", {{.type = mwc::PRT_EDIT, .level = mwc::PRL_AUTOCONFIRMED}});
    Date::advanceFrozenClock(DateDiff::fromMinutes(5));
    assertCopyFails(
        "la feuille de style [[Modèle:Test/styles.css]] a un niveau de protection inférieur à « semi-protection "
        "étendue »");
  }
  CBL_TEST_CASE(ProtectionOfStylesheetExpiringSoon) {
    updatePageContent(TEST_SOURCE_PAGE, "<templatestyles src=\"Test/styles.css\"></templatestyles>");
    m_wiki.setPageProtection(
        "Modèle:Test/styles.css",
        {{.type = mwc::PRT_EDIT, .level = mwc::PRL_AUTOPATROLLED, .expiry = Date::now() + DateDiff::fromDays(2)}});
    Date::advanceFrozenClock(DateDiff::fromMinutes(5));
    assertCopyFails("la protection de la feuille de style [[Modèle:Test/styles.css]] expire dans moins de 3 jours");
  }

  CBL_TEST_CASE(DailyUpdates) {
    m_wiki.setPageContent("Wikipédia:Accueil principal/Image du jour (copie sans modèles)", INITIAL_TARGET_CONTENT);
    m_wiki.setPageContent("Wikipédia:Accueil principal/Éphéméride (copie sans modèles)", INITIAL_TARGET_CONTENT);
    m_wiki.setPageContent("Wikipédia:Accueil principal/Lumière sur (copie sans modèles)", INITIAL_TARGET_CONTENT);
    m_wiki.setPageContent("Wikipédia:Accueil principal/Lumière sur 2 (copie sans modèles)", INITIAL_TARGET_CONTENT);
    updatePageContent("Wikipédia:Image du jour/2 janvier 2001", "[[File:TestA.jpg]]");
    updatePageContent("Wikipédia:Image du jour/1 février 2001", "[[File:TestB.jpg]]");
    updatePageContent("Wikipédia:Éphéméride/2 janvier", "Some fact for January 2.");
    updatePageContent("Wikipédia:Éphéméride/1er février", "Some fact for February 1.");
    updatePageContent("Wikipédia:Lumière sur/Janvier 2001", "{{Lumière sur/Accueil|02a=ArticleOfTheDayJanuary}}");
    updatePageContent("Wikipédia:Lumière sur/Février 2001",
                      "{{Lumière sur/Accueil|01a=ArticleOfTheDayFebruaryA|01b=ArticleOfTheDayFebruaryB}}");
    updatePageContent("Wikipédia:Lumière sur/ArticleOfTheDayJanuary", "Content of ArticleOfTheDayJanuary");
    updatePageContent("Wikipédia:Lumière sur/ArticleOfTheDayFebruaryA", "Content of ArticleOfTheDayFebruaryA");
    updatePageContent("Wikipédia:Lumière sur/ArticleOfTheDayFebruaryB", "Content of ArticleOfTheDayFebruaryB");

    Date::setFrozenValueOfNow(Date::fromISO8601("2001-01-01T20:00:00Z"));
    callUpdateMainPage();
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Accueil principal/Image du jour (copie sans modèles)"),
                  INITIAL_TARGET_CONTENT);
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Accueil principal/Éphéméride (copie sans modèles)"),
                  INITIAL_TARGET_CONTENT);
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Accueil principal/Lumière sur (copie sans modèles)"),
                  INITIAL_TARGET_CONTENT);
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Accueil principal/Lumière sur 2 (copie sans modèles)"),
                  INITIAL_TARGET_CONTENT);
    CBL_ASSERT_EQ(getReportedErrors(), "no reported errors");

    int oldExpandTemplatesCallCount = m_wiki.expandTemplatesCallCount;
    Date::setFrozenValueOfNow(Date::fromISO8601("2001-01-02T00:00:00Z"));
    callUpdateMainPage();
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Accueil principal/Image du jour (copie sans modèles)"),
                  wrapInBotSection("[[File:TestA.jpg]]"));
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Accueil principal/Éphéméride (copie sans modèles)"),
                  wrapInBotSection("Some fact for January 2."));
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Accueil principal/Lumière sur (copie sans modèles)"),
                  wrapInBotSection("Content of ArticleOfTheDayJanuary"));
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Accueil principal/Lumière sur 2 (copie sans modèles)"),
                  wrapInBotSection("<!-- Pas de second article mis en lumière aujourd'hui -->"));
    CBL_ASSERT_EQ(m_wiki.expandTemplatesCallCount, oldExpandTemplatesCallCount);
    CBL_ASSERT_EQ(getReportedErrors(), "no reported errors");

    Date::setFrozenValueOfNow(Date::fromISO8601("2001-02-01T00:00:00Z"));
    callUpdateMainPage();
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Accueil principal/Image du jour (copie sans modèles)"),
                  wrapInBotSection("[[File:TestB.jpg]]"));
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Accueil principal/Éphéméride (copie sans modèles)"),
                  wrapInBotSection("Some fact for February 1."));
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Accueil principal/Lumière sur (copie sans modèles)"),
                  wrapInBotSection("Content of ArticleOfTheDayFebruaryA"));
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Accueil principal/Lumière sur 2 (copie sans modèles)"),
                  wrapInBotSection("Content of ArticleOfTheDayFebruaryB"));
    CBL_ASSERT_EQ(m_wiki.expandTemplatesCallCount, oldExpandTemplatesCallCount);
    CBL_ASSERT_EQ(getReportedErrors(), "no reported errors");
  }

  CBL_TEST_CASE(FeaturedArticlesBadParams) {
    m_wiki.setPageContent("Wikipédia:Accueil principal/Lumière sur (copie sans modèles)", INITIAL_TARGET_CONTENT);
    m_wiki.setPageContent("Wikipédia:Accueil principal/Lumière sur 2 (copie sans modèles)", INITIAL_TARGET_CONTENT);
    updatePageContent("Wikipédia:Lumière sur/Janvier 2001", "{{Lumière sur/Accueil|01a=Unused|02a=|03a=Unused}}");
    Date::setFrozenValueOfNow(Date::fromISO8601("2001-01-02T00:00:00Z"));

    callUpdateMainPage();

    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Accueil principal/Lumière sur (copie sans modèles)"),
                  INITIAL_TARGET_CONTENT);
    CBL_ASSERT_EQ(m_wiki.readPageContent("Wikipédia:Accueil principal/Lumière sur 2 (copie sans modèles)"),
                  INITIAL_TARGET_CONTENT);
    CBL_ASSERT(getReportedErrors().find("Impossible de lire les articles mis en lumière du jour à partir de "
                                        "[[Wikipédia:Lumière sur/Janvier 2001]] : aucun article n'est renseigné pour "
                                        "aujourd'hui") != string::npos)
        << getReportedErrors();
  }

  MockWikiWithParse m_wiki;
  json::Value m_state;
  live_replication::MockRecentChangesReader m_recentChangesReader;
  unique_ptr<TemplateExpansionCache> m_templateExpansionCache;
};

int main() {
  UpdateMainPageTest().run();
  return 0;
}
