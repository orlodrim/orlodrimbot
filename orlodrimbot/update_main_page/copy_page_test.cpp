#include "copy_page.h"
#include <re2/re2.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "cbl/date.h"
#include "cbl/file.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "cbl/tempfile.h"
#include "cbl/unittest.h"
#include "mwclient/mock_wiki.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/mock_recent_changes_reader.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::PageProtection;
using std::string;
using std::unordered_map;
using std::vector;

constexpr const char INITIAL_TARGET_CONTENT[] = "<!-- BEGIN BOT SECTION --><!-- END BOT SECTION -->";

string wrapInBotSection(const string& content) {
  return cbl::concat("<!-- BEGIN BOT SECTION -->\n", content, "\n<!-- END BOT SECTION -->");
}

class MockWikiWithParse : public mwc::MockWiki {
public:
  string expandTemplates(const string& code, const string& title, mwc::revid_t revid) {
    return cbl::replace(code, "{{", "{{expanded:");
  }
  json::Value apiGetRequest(const string& request) override {
    // Simulates the result of the custom parse request by extracting templates directly mentioned in the wikicode.
    CBL_ASSERT_EQ(
        request,
        "action=parse&prop=templates&text=%7B%7BMod%C3%A8le%3ASource%7D%7D&title=Wikip%C3%A9dia%3AAccueil%20principal");

    static const re2::RE2 reTemplate(R"(\{\{([^{}]+)\}\})");
    string code = readPageContent("Modèle:Source");
    re2::StringPiece toParse = code;
    string templateName;
    json::Value result;
    json::Value& templates = result.getMutable("parse").getMutable("templates");
    templates.setToEmptyArray();
    templates.addItem().getMutable("*") = "Modèle:Source";
    while (RE2::FindAndConsume(&toParse, reTemplate, &templateName)) {
      templates.addItem().getMutable("*") = normalizeTitle(templateName, mwc::NS_TEMPLATE);
    }
    return result;
  }
};

class CopyPageTest : public cbl::Test {
private:
  void setUp() override {
    m_wiki.resetDatabase();

    Date::setFrozenValueOfNow(Date::fromISO8601("2001-01-01T10:00:00Z"));
    m_recentChangesReader.resetMock();
    m_wiki.setPageContent("Modèle:Source", ".");
    m_wiki.setPageContent("Modèle:Target", INITIAL_TARGET_CONTENT);
    cbl::writeFile(m_stateFile.path(), "{}");
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
  void assertCopyFails(const string& expectedErrorMessage) {
    string oldContent = m_wiki.readPageContent("Modèle:Target");
    string errorMessage;
    try {
      copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                      "Modèle:Target");
    } catch (const CopyError& error) {
      errorMessage = error.what();
    }
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"), oldContent);
    CBL_ASSERT_EQ(errorMessage, expectedErrorMessage);
  }
  CBL_TEST_CASE(StandardUpdates) {
    updatePageContent("Modèle:Source", "Test content.");

    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                    "Modèle:Target");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"), wrapInBotSection("Test content."));

    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(1));
    updatePageContent("Modèle:Source", "Test content 2.");

    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                    "Modèle:Target");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"), wrapInBotSection("Test content 2."));
  }
  CBL_TEST_CASE(RemoveNoinclude) {
    updatePageContent("Modèle:Source", "Test content.<noinclude>{{Documentation}}</noinclude>");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                    "Modèle:Target");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"), wrapInBotSection("Test content."));
  }
  CBL_TEST_CASE(MissingBotSection) {
    m_wiki.setPageContent("Modèle:Target", ".");
    updatePageContent("Modèle:Source", "Test content");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    assertCopyFails("Section de bot non trouvée sur [[Modèle:Target]]");
  }
  CBL_TEST_CASE(NoUpdateJustAfterEdit) {
    updatePageContent("Modèle:Source", "Test content.");
    copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                    "Modèle:Target");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"), INITIAL_TARGET_CONTENT);
  }
  CBL_TEST_CASE(ExpandTemplate) {
    updatePageContent("Modèle:Abc", "Value");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    updatePageContent("Modèle:Source", "{{abc}}");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                    "Modèle:Target");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"), wrapInBotSection("{{expanded:abc}}"));
  }
  CBL_TEST_CASE(NoUpdateDueToRecentlyModifiedTemplate) {
    updatePageContent("Modèle:Source", "{{abc}}");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(1));
    updatePageContent("Modèle:Abc", "Value");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    assertCopyFails("Le modèle récemment modifié [[:Modèle:Abc]] est inclus dans [[Modèle:Source]]");
  }
  CBL_TEST_CASE(MultipleTemplates) {
    updatePageContent("Modèle:Abc", "Value1");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    updatePageContent("Modèle:Source", "{{abc}} {{def}}");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(1));
    updatePageContent("Modèle:Def", "Value2");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    assertCopyFails("Le modèle récemment modifié [[:Modèle:Def]] est inclus dans [[Modèle:Source]]");
  }
  CBL_TEST_CASE(SkipUpdateIfNoRecentChange) {
    m_wiki.setPageContent("Modèle:Source", "Test content.");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                    "Modèle:Target");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"), INITIAL_TARGET_CONTENT);
  }
  CBL_TEST_CASE(ProtectedStylesheets) {
    updatePageContent("Modèle:Source",
                      "<templatestyles src=\"Modèle:Test/styles.css\"></templatestyles>\n"
                      "<templatestyles src=\"Modèle:Test2/styles.css\"></templatestyles>");
    m_wiki.setPageProtection("Modèle:Test/styles.css", {{.type = mwc::PRT_EDIT, .level = mwc::PRL_AUTOPATROLLED}});
    m_wiki.setPageProtection(
        "Modèle:Test2/styles.css",
        {{.type = mwc::PRT_EDIT, .level = mwc::PRL_SYSOP, .expiry = Date::now() + DateDiff::fromDays(10)}});
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                    "Modèle:Target");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"),
                  wrapInBotSection("<templatestyles src=\"Modèle:Test/styles.css\"></templatestyles>\n"
                                   "<templatestyles src=\"Modèle:Test2/styles.css\"></templatestyles>"));
  }
  CBL_TEST_CASE(UnprotectedStylesheet) {
    updatePageContent("Modèle:Source", "<templatestyles src=\"Modèle:Test/styles.css\"></templatestyles>");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    assertCopyFails("la feuille de style [[Modèle:Test/styles.css]] n'est pas protégée");
  }
  CBL_TEST_CASE(InsufficientlyProtectedStylesheet) {
    updatePageContent("Modèle:Source", "<templatestyles src=\"Modèle:Test/styles.css\"></templatestyles>");
    m_wiki.setPageProtection("Modèle:Test/styles.css", {{.type = mwc::PRT_EDIT, .level = mwc::PRL_AUTOCONFIRMED}});
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    assertCopyFails(
        "la feuille de style [[Modèle:Test/styles.css]] a un niveau de protection inférieur à « semi-protection "
        "étendue »");
  }
  CBL_TEST_CASE(ProtectionOfStylesheetExpiringSoon) {
    updatePageContent("Modèle:Source", "<templatestyles src=\"Modèle:Test/styles.css\"></templatestyles>");
    m_wiki.setPageProtection(
        "Modèle:Test/styles.css",
        {{.type = mwc::PRT_EDIT, .level = mwc::PRL_AUTOPATROLLED, .expiry = Date::now() + DateDiff::fromDays(2)}});
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    assertCopyFails("la protection de la feuille de style [[Modèle:Test/styles.css]] expire dans moins de 3 jours");
  }

  MockWikiWithParse m_wiki;
  live_replication::MockRecentChangesReader m_recentChangesReader;
  cbl::TempFile m_stateFile;
};

int main() {
  CopyPageTest().run();
  return 0;
}
