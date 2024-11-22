#include "template_expansion_cache.h"
#include <memory>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "cbl/unittest.h"
#include "mock_wiki_with_parse.h"

using cbl::Date;
using cbl::DateDiff;
using std::make_unique;
using std::unique_ptr;

class TemplateExpansionCacheTest : public cbl::Test {
private:
  void setUp() override {
    m_wiki.resetDatabase();
    m_templateExpansionCache = make_unique<TemplateExpansionCache>(&m_wiki, ":memory:");
  }

  CBL_TEST_CASE(ExpandAndUseCache) {
    Date::setFrozenValueOfNow(Date::fromISO8601("2001-01-01T01:00:00Z"));
    m_wiki.setPageContent("Modèle:A", ".");
    Date::setFrozenValueOfNow(Date::fromISO8601("2001-01-01T02:00:00Z"));
    m_wiki.setPageContent("Modèle:C", ".");
    Date::setFrozenValueOfNow(Date::fromISO8601("2001-01-01T03:00:00Z"));
    m_wiki.setPageContent("Modèle:B", ".");

    // The expansion needs to be requested from the wiki when it is requested for the first time.
    {
      Date::advanceFrozenClock(DateDiff::fromDays(1));
      ExpansionResult result = m_templateExpansionCache->expand("Test: {{a}}, {{b}}, {{c}}", "TestPage", 123);
      CBL_ASSERT_EQ(result.code, "Test: {{expanded:a}}, {{expanded:b}}, {{expanded:c}}");
      CBL_ASSERT_EQ(cbl::join(result.templates, ","), "Modèle:A,Modèle:B,Modèle:C");
      CBL_ASSERT_EQ(result.lastChangedTemplate, "Modèle:B");
      CBL_ASSERT_EQ(result.lastChangedTemplateTimestamp, Date::fromISO8601("2001-01-01T03:00:00Z"));
      CBL_ASSERT_EQ(result.fromCache, false);
      CBL_ASSERT_EQ(m_wiki.expandTemplatesCallCount, 1);
    }

    // The expansion is now in cache.
    {
      Date::advanceFrozenClock(DateDiff::fromDays(1));
      ExpansionResult result = m_templateExpansionCache->expand("Test: {{a}}, {{b}}, {{c}}", "TestPage", 123);
      CBL_ASSERT_EQ(result.code, "Test: {{expanded:a}}, {{expanded:b}}, {{expanded:c}}");
      CBL_ASSERT_EQ(cbl::join(result.templates, ","), "Modèle:A,Modèle:B,Modèle:C");
      CBL_ASSERT_EQ(result.lastChangedTemplate, "Modèle:B");
      CBL_ASSERT_EQ(result.lastChangedTemplateTimestamp, Date::fromISO8601("2001-01-01T03:00:00Z"));
      CBL_ASSERT_EQ(result.fromCache, true);
      CBL_ASSERT_EQ(m_wiki.expandTemplatesCallCount, 1);
    }

    // The cache expires after a bit less than a year.
    {
      Date::advanceFrozenClock(DateDiff::fromDays(360));
      m_templateExpansionCache->expand("Test: {{a}}, {{b}}, {{c}}", "TestPage", 123);
      CBL_ASSERT_EQ(m_wiki.expandTemplatesCallCount, 2);
    }
  }

  CBL_TEST_CASE(PageNameAndRevid) {
    Date::advanceFrozenClock(DateDiff::fromDays(1));
    ExpansionResult result = m_templateExpansionCache->expand("{{PAGENAME}} {{REVISIONID}}", "TestPage", 123);
    CBL_ASSERT_EQ(result.code, "TestPage 123");
  }

  MockWikiWithParse m_wiki;
  unique_ptr<TemplateExpansionCache> m_templateExpansionCache;
};

int main() {
  TemplateExpansionCacheTest().run();
  return 0;
}
