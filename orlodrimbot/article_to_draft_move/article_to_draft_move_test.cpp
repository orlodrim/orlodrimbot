#include "article_to_draft_move.h"
#include <string>
#include <vector>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/unittest.h"
#include "mwclient/mock_wiki.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/wiki_job_runner/job_queue/job_queue.h"

using cbl::Date;
using mwc::LogEvent;
using mwc::Wiki;
using std::string;

bool isArticleToDraftMove(Wiki& wiki, const LogEvent& logEvent);

class MockWikiWithEditCount : public mwc::MockWiki {
public:
  void getUsersInfo(int properties, std::vector<mwc::UserInfo>& users) override {
    for (mwc::UserInfo& user : users) {
      user.editCount = 1;
    }
  }
};

class ArticleToDraftMoveTest : public cbl::Test {
private:
  void setUp() { m_wiki.resetDatabase(); }

  void runJob(const string& article, const string& draft, const string& mover, const string& reason = "",
              const Date& moveTimestamp = Date()) {
    ArticleToDraftMoveHandler moveHandler(&m_wiki);
    job_queue::JobQueue jobQueue(":memory:");
    job_queue::Job job;
    job.key = draft;
    job.parameters.getMutable("article") = article;
    job.parameters.getMutable("mover") = mover;
    job.parameters.getMutable("move_timestamp") = moveTimestamp.toISO8601();
    job.parameters.getMutable("comment") = reason;
    moveHandler.run(job, jobQueue, false);
  }

  CBL_TEST_CASE(BaseCase) {
    m_wiki.setPageContent("Utilisateur:MockUser/DraftWithCustomName", "...");
    runJob("OriginalArticle", "Utilisateur:MockUser/DraftWithCustomName", "AdminUser", "Reason X");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Utilisateur:MockUser/Brouillon"),
                  "{{Lien vers article transformé en brouillon"
                  "|article=OriginalArticle"
                  "|utilisateur=AdminUser"
                  "|brouillon=Utilisateur:MockUser/DraftWithCustomName}}");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:MockUser"),
                  "{{subst:Utilisateur:OrlodrimBot/Message article transformé en brouillon"
                  "|article=OriginalArticle"
                  "|utilisateur=AdminUser"
                  "|brouillon=Utilisateur:MockUser/DraftWithCustomName"
                  "|commentaire=Reason X}}");
  }

  CBL_TEST_CASE(AlreadyInStandardDraftPage) {
    m_wiki.setPageContent("Utilisateur:MockUser/Brouillon", "...");
    runJob("OriginalArticle", "Utilisateur:MockUser/Brouillon", "AdminUser", "Reason X");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Utilisateur:MockUser/Brouillon"), "...");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:MockUser"),
                  "{{subst:Utilisateur:OrlodrimBot/Message article transformé en brouillon"
                  "|article=OriginalArticle"
                  "|utilisateur=AdminUser"
                  "|brouillon=Utilisateur:MockUser/Brouillon"
                  "|commentaire=Reason X}}");
  }

  CBL_TEST_CASE(TemporaryUser) {
    m_wiki.setPageContent("Utilisateur:~2025-12345-6/DraftWithCustomName", "...", "~2025-12345-6");
    runJob("OriginalArticle", "Utilisateur:~2025-12345-6/DraftWithCustomName", "AdminUser", "Reason X");
    CBL_ASSERT(!m_wiki.pageExists("Utilisateur:~2025-12345-6/Brouillon"));
    CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:~2025-12345-6"),
                  "{{subst:Utilisateur:OrlodrimBot/Message article transformé en brouillon"
                  "|article=OriginalArticle"
                  "|utilisateur=AdminUser"
                  "|brouillon=Utilisateur:~2025-12345-6/DraftWithCustomName"
                  "|commentaire=Reason X}}");
  }

  CBL_TEST_CASE(BotExclusion) {
    m_wiki.setPageContent("Discussion utilisateur:MockUser", "{{bots|optout=article-vers-brouillon}}");
    m_wiki.setPageContent("Utilisateur:MockUser/DraftWithCustomName", "...");
    runJob("OriginalArticle", "Utilisateur:MockUser/DraftWithCustomName", "AdminUser", "Reason X");
    CBL_ASSERT(!m_wiki.pageExists("Utilisateur:MockUser/Brouillon"));
    CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:MockUser"), "{{bots|optout=article-vers-brouillon}}");
  }
  CBL_TEST_CASE(BotExclusion2) {
    m_wiki.setPageContent("Utilisateur:MockUser/DraftWithCustomName", "...");
    m_wiki.setPageContent("Utilisateur:MockUser/Brouillon", "{{bots|optout=article-vers-brouillon}}");
    runJob("OriginalArticle", "Utilisateur:MockUser/DraftWithCustomName", "AdminUser", "Reason X");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Utilisateur:MockUser/Brouillon"), "{{bots|optout=article-vers-brouillon}}");
  }

  CBL_TEST_CASE(EscapeComment) {
    m_wiki.setPageContent("Utilisateur:MockUser/DraftWithCustomName", "...");
    runJob("OriginalArticle", "Utilisateur:MockUser/DraftWithCustomName", "AdminUser",
           "Only contains [[Category:Nothing]]");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:MockUser"),
                  "{{subst:Utilisateur:OrlodrimBot/Message article transformé en brouillon"
                  "|article=OriginalArticle"
                  "|utilisateur=AdminUser"
                  "|brouillon=Utilisateur:MockUser/DraftWithCustomName"
                  "|commentaire=Only contains [[:Category:Nothing]]}}");
  }

  CBL_TEST_CASE(SkipMessageBecauseTalkPageHasLink) {
    m_wiki.setPageContent("Discussion utilisateur:MockUser", "[[Utilisateur:MockUser/DraftWithCustomName]]");
    m_wiki.setPageContent("Utilisateur:MockUser/DraftWithCustomName", "...");
    runJob("OriginalArticle", "Utilisateur:MockUser/DraftWithCustomName", "AdminUser");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:MockUser"),
                  "[[Utilisateur:MockUser/DraftWithCustomName]]");
  }

  CBL_TEST_CASE(SkipMessageBecauseTalkPageHasRecentSpecialIndexLink) {
    string initialTalkPageContent = "[[Spécial:Index/Utilisateur:MockUser/]] --AdminUser 1 janvier 2000 à 13:01 (CET)";
    m_wiki.setPageContent("Discussion utilisateur:MockUser", initialTalkPageContent);
    m_wiki.setPageContent("Utilisateur:MockUser/DraftWithCustomName", "...");
    runJob("OriginalArticle", "Utilisateur:MockUser/DraftWithCustomName", "AdminUser", "",
           Date::fromISO8601("2000-01-01T12:00:00Z"));
    CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:MockUser"), initialTalkPageContent);
  }

  CBL_TEST_CASE(SkipMessageBecauseTalkPageIsRedirect) {
    m_wiki.setPageContent("Discussion utilisateur:MockUser", "#REDIRECTION [[Discussion utilisateur:MockUser2]]");
    m_wiki.setPageContent("Utilisateur:MockUser/DraftWithCustomName", "...");
    runJob("OriginalArticle", "Utilisateur:MockUser/DraftWithCustomName", "AdminUser");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Discussion utilisateur:MockUser"),
                  "#REDIRECTION [[Discussion utilisateur:MockUser2]]");
  }

  CBL_TEST_CASE(AddToExistingDraftSubpage) {
    m_wiki.setPageContent("Utilisateur:MockUser/DraftWithCustomName", "...");
    m_wiki.setPageContent("Utilisateur:MockUser/Brouillon", "{{Brouillon}}");
    runJob("OriginalArticle", "Utilisateur:MockUser/DraftWithCustomName", "AdminUser");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Utilisateur:MockUser/Brouillon"),
                  "{{Lien vers article transformé en brouillon"
                  "|article=OriginalArticle"
                  "|utilisateur=AdminUser"
                  "|brouillon=Utilisateur:MockUser/DraftWithCustomName}}\n"
                  "{{Brouillon}}");
  }

  CBL_TEST_CASE(ReplaceDraftRedirectingToMain) {
    m_wiki.setPageContent("Utilisateur:MockUser/DraftWithCustomName", "...");
    m_wiki.setPageContent("Utilisateur:MockUser/Brouillon", "#REDIRECTION [[SomeOtherArticle]]");
    runJob("OriginalArticle", "Utilisateur:MockUser/DraftWithCustomName", "AdminUser");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Utilisateur:MockUser/Brouillon"),
                  "{{Lien vers article transformé en brouillon"
                  "|article=OriginalArticle"
                  "|utilisateur=AdminUser"
                  "|brouillon=Utilisateur:MockUser/DraftWithCustomName}}");
  }

  CBL_TEST_CASE(DoNotReplaceDraftRedirectingToOtherNamespace) {
    m_wiki.setPageContent("Utilisateur:MockUser/DraftWithCustomName", "...");
    m_wiki.setPageContent("Utilisateur:MockUser/Brouillon", "#REDIRECTION [[Utilisateur:MockUser/Brouillon2]]");
    runJob("OriginalArticle", "Utilisateur:MockUser/DraftWithCustomName", "AdminUser");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Utilisateur:MockUser/Brouillon"),
                  "#REDIRECTION [[Utilisateur:MockUser/Brouillon2]]");
  }

  CBL_TEST_CASE(SkipMessageBecauseUserIsMovingTheirOwnArticle) {
    m_wiki.setPageContent("Utilisateur:MockUser/DraftWithCustomName", "...");
    runJob("OriginalArticle", "Utilisateur:MockUser/DraftWithCustomName", "MockUser", "Reason X");
    CBL_ASSERT(!m_wiki.pageExists("Utilisateur:MockUser/Brouillon"));
    CBL_ASSERT(!m_wiki.pageExists("Discussion utilisateur:MockUser"));
  }

  CBL_TEST_CASE(DoNotAddTooManyTemplates) {
    m_wiki.setPageContent("Utilisateur:MockUser/DraftWithCustomName", "...");
    m_wiki.setPageContent("Utilisateur:MockUser/Brouillon",
                          "{{Lien vers article transformé en brouillon|1}}\n"
                          "{{Lien vers article transformé en brouillon|2}}\n"
                          "{{Lien vers article transformé en brouillon|3}}\n"
                          "{{Lien vers article transformé en brouillon|4}}\n"
                          "{{Lien vers article transformé en brouillon|5}}");
    runJob("OriginalArticle", "Utilisateur:MockUser/DraftWithCustomName", "AdminUser");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Utilisateur:MockUser/Brouillon"),
                  "{{Lien vers article transformé en brouillon|1}}\n"
                  "{{Lien vers article transformé en brouillon|2}}\n"
                  "{{Lien vers article transformé en brouillon|3}}\n"
                  "{{Lien vers article transformé en brouillon|4}}\n"
                  "{{Lien vers article transformé en brouillon|5}}");
  }

  CBL_TEST_CASE(DoNotAddTheSameTemplateTwice) {
    m_wiki.setPageContent("Utilisateur:MockUser/DraftWithCustomName", "...");
    m_wiki.setPageContent("Utilisateur:MockUser/Brouillon",
                          "{{Lien vers article transformé en brouillon"
                          "|brouillon=Utilisateur:MockUser/DraftWithCustomName}}");
    runJob("OriginalArticle", "Utilisateur:MockUser/DraftWithCustomName", "AdminUser");
    CBL_ASSERT_EQ(m_wiki.readPageContent("Utilisateur:MockUser/Brouillon"),
                  "{{Lien vers article transformé en brouillon"
                  "|brouillon=Utilisateur:MockUser/DraftWithCustomName}}");
  }

  MockWikiWithEditCount m_wiki;
};

int main() {
  ArticleToDraftMoveTest().run();
  return 0;
}
