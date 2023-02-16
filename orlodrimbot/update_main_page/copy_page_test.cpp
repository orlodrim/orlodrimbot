#include "copy_page.h"
#include <re2/re2.h>
#include <string>
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
using std::string;

class MockWikiWithParse : public mwc::MockWiki {
public:
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

constexpr const char TARGET_HEADER[] =
    "<!-- Cette page est mise à jour automatiquement à partir de [[Modèle:Source]]. "
    "Les changements faits directement ici seront écrasés. -->\n";

class CopyPageTest : public cbl::Test {
private:
  void setUp() override {
    m_wiki.resetDatabase();
    m_recentChangesReader.resetMock();
    m_wiki.setPageContent("Modèle:Source", ".");
    m_wiki.setPageContent("Modèle:Target", ".");
    cbl::writeFile(m_stateFile.path(), "{}");
    Date::setFrozenValueOfNow(Date::fromISO8601("2001-01-01T10:00:00Z"));
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
  CBL_TEST_CASE(StandardUpdates) {
    updatePageContent("Modèle:Source", "Test content.");

    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                    "Modèle:Target");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"), cbl::concat(TARGET_HEADER, "Test content."));

    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(1));
    updatePageContent("Modèle:Source", "Test content 2.");

    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                    "Modèle:Target");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"), cbl::concat(TARGET_HEADER, "Test content 2."));
  }
  CBL_TEST_CASE(RemoveDocSuffix) {
    updatePageContent("Modèle:Source", "Test content.<noinclude>{{Documentation}}</noinclude>");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                    "Modèle:Target");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"), cbl::concat(TARGET_HEADER, "Test content."));
  }
  CBL_TEST_CASE(NoUpdateJustAfterEdit) {
    updatePageContent("Modèle:Source", "Test content.");
    copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                    "Modèle:Target");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"), ".");
  }
  CBL_TEST_CASE(UpdateWithTemplate) {
    updatePageContent("Modèle:Abc", "Value");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    updatePageContent("Modèle:Source", "{{abc}}");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                    "Modèle:Target");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"), cbl::concat(TARGET_HEADER, "{{abc}}"));
  }
  CBL_TEST_CASE(NoUpdateDueToRecentlyModifiedTemplate) {
    updatePageContent("Modèle:Source", "{{abc}}");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(1));
    updatePageContent("Modèle:Abc", "Value");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    string errorMessage;
    try {
      copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                      "Modèle:Target");
    } catch (const CopyError& error) {
      errorMessage = error.what();
    }
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"), ".");
    CBL_ASSERT_EQ(errorMessage, "Le modèle récemment modifié [[:Modèle:Abc]] est inclus dans [[Modèle:Source]]");
  }
  CBL_TEST_CASE(MultipleTemplates) {
    updatePageContent("Modèle:Abc", "Value1");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    updatePageContent("Modèle:Source", "{{abc}} {{def}}");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(1));
    updatePageContent("Modèle:Def", "Value2");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    string errorMessage;
    try {
      copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                      "Modèle:Target");
    } catch (const CopyError& error) {
      errorMessage = error.what();
    }
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"), ".");
    CBL_ASSERT_EQ(errorMessage, "Le modèle récemment modifié [[:Modèle:Def]] est inclus dans [[Modèle:Source]]");
  }
  CBL_TEST_CASE(SkipUpdateIfNoRecentChange) {
    m_wiki.setPageContent("Modèle:Source", "Test content.");
    Date::setFrozenValueOfNow(Date::now() + DateDiff::fromMinutes(5));
    copyPageIfTemplatesAreUnchanged(m_wiki, &m_recentChangesReader, m_stateFile.path(), "Modèle:Source",
                                    "Modèle:Target");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Modèle:Target"), ".");
  }

  MockWikiWithParse m_wiki;
  live_replication::MockRecentChangesReader m_recentChangesReader;
  cbl::TempFile m_stateFile;
};

int main() {
  CopyPageTest().run();
  return 0;
}
