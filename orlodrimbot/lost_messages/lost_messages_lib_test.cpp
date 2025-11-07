#include "lost_messages_lib.h"
#include <re2/re2.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "cbl/containers_helpers.h"
#include "cbl/date.h"
#include "cbl/file.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "cbl/tempfile.h"
#include "cbl/unittest.h"
#include "mwclient/mock_wiki.h"
#include "mwclient/wiki.h"
#include "message_classifier.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::Revision;
using mwc::UserContribsParams;
using std::make_unique;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

constexpr string_view LIST_PAGE = "Utilisateur:OrlodrimBot/Messages sans destinataire";

class CustomMockWiki : public mwc::MockWiki {
public:
  vector<Revision> getUserContribs(const UserContribsParams& params) override {
    vector<Revision> result;
    const string* activity = cbl::findOrNull(inactiveUsers, params.user);
    if (activity == nullptr) {
      result.push_back({.timestamp = Date::now()});
    } else if (*activity == "inactive") {
      result.push_back({.timestamp = Date::now() - DateDiff::fromDays(30)});
    } else if (*activity == "missing") {
      // Keep result empty.
    } else {
      CBL_FATAL << *activity;
    }
    return result;
  }
  unordered_map<string, string> inactiveUsers;
};

class MockMessageClassifier : public MessageClassifier {
public:
  MessageClassification classify(string_view message) const override {
    MessageClassification classification;
    string categoryString, languageString;
    re2::RE2::PartialMatch(message, R"(llmcat=(\w+))", &categoryString);
    classification.llmCategory = getCategoryOfString(categoryString);
    re2::RE2::PartialMatch(message, R"(lang=(\w+))", &languageString);
    classification.llmLanguage =
        languageString.empty() ? MessageClassification::Language::FRENCH : getLanguageOfString(languageString);
    classification.localModelCategory = message.find('?') != string_view::npos
                                            ? MessageClassification::Category::WIKI_QUESTION
                                            : MessageClassification::Category::OTHER;
    classification.llmBlocked = message.find("blocked=true") != string::npos;
    return classification;
  }
};

class LostMessagesTest : public cbl::Test {
private:
  void setUp() {
    Date::setFrozenValueOfNow(Date::fromISO8601("2025-01-02T03:04:05Z"));
    m_wiki = make_unique<CustomMockWiki>();
    m_lostMessages = make_unique<LostMessages>(m_wiki.get(), "", make_unique<MockMessageClassifier>());
    m_wiki->setPageContent(cbl::legacyStringConv(LIST_PAGE), "<!-- BEGIN BOT SECTION --><!-- END BOT SECTION -->");
  }

  void postMessage(const string& message, const string& user) {
    string talkPage = "Discussion utilisateur:TestUser";
    string oldContent = m_wiki->pageExists(talkPage) ? m_wiki->readPageContent(talkPage) : "";
    string newContent = oldContent.empty() ? message : cbl::concat(oldContent, "\n", message);
    m_wiki->setPageContent(talkPage, newContent, user);
  }
  CBL_TEST_CASE(NotAQuestion) {
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    postMessage(":Merci ! (llmcat=Thanks) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_wiki->setPageContent(string(LIST_PAGE),
                           "<!-- BEGIN BOT SECTION -->\n* Old line 1\n* Old line 2\n<!-- END BOT SECTION -->");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent(LIST_PAGE), cbl::unindent(R"(
        <!-- BEGIN BOT SECTION -->
        * 2 janvier 2025 à 04:04 : {{u|TestUser}} a [[Spécial:Diff/3|modifié]] sa page de discussion (catégorisation : modèle local = autre / LLM = remerciements)
        * Old line 1
        * Old line 2
        <!-- END BOT SECTION -->)"));
  }
  CBL_TEST_CASE(QuestionUnknownSection) {
    postMessage("Message. [[Utilisateur:NaggoBot|NaggoBot]]", "NaggoBot");
    postMessage(":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
        Message. [[Utilisateur:NaggoBot|NaggoBot]]
        :Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]
        ::Bonjour TestUser,
        ::Je suis un robot de Wikipédia. J'ai remarqué que vous avez écrit sur votre propre page de discussion alors que celle-ci ne contient que des messages de robots. Nous les robots ne pouvons pas comprendre les messages qui nous sont écrits ! '''Aucun humain ne sera prévenu de votre message''' si vous ne le postez pas à l'endroit approprié.
        ::Vous pouvez par exemple vous adresser au [[Wikipédia:Forum des nouveaux|forum des nouveaux]].
        ::~~~~)"));
    CBL_ASSERT_EQ(m_wiki->readPageContent(LIST_PAGE), cbl::unindent(R"(
        <!-- BEGIN BOT SECTION -->
        * '''2 janvier 2025 à 04:04 : {{u|TestUser}} a [[Spécial:Diff/3|modifié]] sa page de discussion (catégorisation : modèle local = question / LLM = question)'''
        <!-- END BOT SECTION -->)"));
  }
  CBL_TEST_CASE(QuestionOnDraftTalkPage) {
    CBL_INFO << "==== QuestionOnDraftTalkPage ====";
    m_wiki->setPageContent("Discussion utilisateur:TestUser/Brouillon",
                           "Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_lostMessages->runForUser("TestUser", /* onDraftPage = */ true);
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser/Brouillon"), cbl::unindent(R"(
        Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]
        :Bonjour {{notif-|TestUser}},
        :Je suis un robot de Wikipédia. J'ai remarqué que vous avez écrit sur la page de discussion de votre propre brouillon.
        :En procédant ainsi, '''aucun humain ne sera prévenu de votre message'''. Pour obtenir une réponse, postez votre message sur une page de discussion communautaire ou sur celle d'un autre utilisateur.
        :Par exemple, pour demander une relecture de votre brouillon, postez une demande sur la page [[Wikipédia:Forum de relecture]]. Pour les questions plus générales, vous pouvez vous adresser au [[Wikipédia:Forum des nouveaux|forum des nouveaux]].
        :~~~~)"));
    CBL_ASSERT_EQ(m_wiki->readPageContent(LIST_PAGE), cbl::unindent(R"(
        <!-- BEGIN BOT SECTION -->
        * '''2 janvier 2025 à 04:04 : {{u|TestUser}} a [[Spécial:Diff/2|modifié]] la page de discussion de son brouillon (catégorisation : modèle local = question / LLM = question)'''
        <!-- END BOT SECTION -->)"));
  }
  CBL_TEST_CASE(QuestionOnDraftTalkPageWithLinkToDraft) {
    CBL_INFO << "==== QuestionOnDraftTalkPage ====";
    m_wiki->setPageContent("Discussion utilisateur:TestUser/Brouillon",
                           "Pourriez-vous relire mon brouillon [[Utilisateur: TestUser / Brouillon]] ? "
                           "(llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]",
                           "TestUser");
    m_lostMessages->runForUser("TestUser", /* onDraftPage = */ true);
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser/Brouillon"), cbl::unindent(R"(
        Pourriez-vous relire mon brouillon [[Utilisateur: TestUser / Brouillon]] ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]
        :Bonjour {{notif-|TestUser}},
        :Je suis un robot de Wikipédia. J'ai remarqué que vous avez écrit sur la page de discussion de votre propre brouillon.
        :En procédant ainsi, '''aucun humain ne sera prévenu de votre message'''. Pour obtenir une réponse, postez votre message sur une page de discussion communautaire ou sur celle d'un autre utilisateur.
        :Par exemple, pour demander une relecture de votre brouillon, postez une demande sur la page [[Wikipédia:Forum de relecture]]. Pour les questions plus générales, vous pouvez vous adresser au [[Wikipédia:Forum des nouveaux|forum des nouveaux]].
        :~~~~)"));
    CBL_ASSERT_EQ(m_wiki->readPageContent(LIST_PAGE), cbl::unindent(R"(
        <!-- BEGIN BOT SECTION -->
        * '''2 janvier 2025 à 04:04 : {{u|TestUser}} a [[Spécial:Diff/2|modifié]] la page de discussion de son brouillon (catégorisation : modèle local = question / LLM = question)'''
        <!-- END BOT SECTION -->)"));
  }
  CBL_TEST_CASE(QuestionAfterWelcomeMessage) {
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    postMessage(":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_wiki->setPageContent("Discussion utilisateur:TestMentor", "Header");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
        Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]
        :Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]])"));
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestMentor"), cbl::unindent(R"(
        Header

        == Question de TestUser ==
        Bonjour TestMentor,

        [[Utilisateur:TestUser|TestUser]] a reçu un message de bienvenue signé par vous et y a répondu, mais sans vous notifier. Vous pouvez lire son message sur sa [[Discussion utilisateur:TestUser|page de discussion]] ([[Spécial:Diff/3|voir le diff]]).

        Comme c'est la première fois que je vous envoie une telle notification, voici quelques informations complémentaires :
        * Je suis programmé pour détecter les messages laissés par les nouveaux utilisateurs sur leur page de discussion dont personne ne serait averti autrement.
        * Je les transmets à leur mentor en fonction de certains critères, expliqués sur [[Utilisateur:OrlodrimBot/Messages sans destinataire|cette page]].
        * Pour vous désabonner entièrement de ces notifications, ajoutez {{((}}bots|optout=notif-réponse-bienvenue{{))}} dans l'en-tête de votre page de discussion.

        ~~~~)"));
  }
  CBL_TEST_CASE(QuestionWithMultipleEdits) {
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    m_wiki->setPageContent("Discussion utilisateur:TestUser",
                           "Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]\n"
                           ":Quoi ? (llm=WikiQuestion) [[Utilisateur:TestUser|TestUser]]",
                           "TestUser");
    m_wiki->setPageContent("Discussion utilisateur:TestUser",
                           "Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]\n"
                           ":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]",
                           "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
        Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]
        :Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]
        ::Bonjour TestUser,
        ::Je suis un robot de Wikipédia. J'ai remarqué que vous avez répondu au message d'accueil sur votre propre page de discussion.
        ::Comme le message d'accueil est automatique, TestMentor ne sera pas averti de votre réponse. N'hésitez pas à {{subst:gender:TestMentor|le|la}} contacter sur [[Discussion utilisateur:TestMentor|sa page de discussion]].
        ::~~~~)"));
  }
  CBL_TEST_CASE(KeepTrackOfNotifiedMentors) {
    cbl::TempFile mentorStateFile;
    unique_ptr<LostMessages> lostMessages;

    lostMessages =
        make_unique<LostMessages>(m_wiki.get(), mentorStateFile.path(), make_unique<MockMessageClassifier>());
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    postMessage(":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_wiki->setPageContent("Discussion utilisateur:TestMentor", "Header");
    lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
        Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]
        :Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]])"));
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestMentor"), cbl::unindent(R"(
        Header

        == Question de TestUser ==
        Bonjour TestMentor,

        [[Utilisateur:TestUser|TestUser]] a reçu un message de bienvenue signé par vous et y a répondu, mais sans vous notifier. Vous pouvez lire son message sur sa [[Discussion utilisateur:TestUser|page de discussion]] ([[Spécial:Diff/3|voir le diff]]).

        Comme c'est la première fois que je vous envoie une telle notification, voici quelques informations complémentaires :
        * Je suis programmé pour détecter les messages laissés par les nouveaux utilisateurs sur leur page de discussion dont personne ne serait averti autrement.
        * Je les transmets à leur mentor en fonction de certains critères, expliqués sur [[Utilisateur:OrlodrimBot/Messages sans destinataire|cette page]].
        * Pour vous désabonner entièrement de ces notifications, ajoutez {{((}}bots|optout=notif-réponse-bienvenue{{))}} dans l'en-tête de votre page de discussion.

        ~~~~)"));
    CBL_ASSERT_EQ(cbl::readFile(mentorStateFile.path()), "TestMentor|anything\n");

    lostMessages =
        make_unique<LostMessages>(m_wiki.get(), mentorStateFile.path(), make_unique<MockMessageClassifier>());
    m_wiki->setPageContent("Discussion utilisateur:TestUser2",
                           "Bienvenue sur Wikipédia, TestUser2. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    m_wiki->setPageContent("Discussion utilisateur:TestUser2",
                           "Bienvenue sur Wikipédia, TestUser2. [[Utilisateur:TestMentor|TestMentor]]\n"
                           ":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser2|TestUser2]]",
                           "TestUser2");
    lostMessages->runForUser("TestUser2");
    CBL_ASSERT(m_wiki->readPageContent("Discussion utilisateur:TestMentor").ends_with(cbl::unindent(R"(
        == Question de TestUser2 ==
        Bonjour TestMentor,

        [[Utilisateur:TestUser2|TestUser2]] a reçu un message de bienvenue signé par vous et y a répondu, mais sans vous notifier. Vous pouvez lire son message sur sa [[Discussion utilisateur:TestUser2|page de discussion]] ([[Spécial:Diff/8|voir le diff]]).

        ~~~~)")));

    lostMessages =
        make_unique<LostMessages>(m_wiki.get(), mentorStateFile.path(), make_unique<MockMessageClassifier>());
    m_wiki->setPageContent("Discussion utilisateur:TestUser3",
                           "Bienvenue sur Wikipédia, TestUser3. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    m_wiki->setPageContent("Discussion utilisateur:TestUser3",
                           "Bienvenue sur Wikipédia, TestUser3. [[Utilisateur:TestMentor|TestMentor]]\n"
                           ":Merci ! (llmcat=Thanks) [[Utilisateur:TestUser3|TestUser3]]",
                           "TestUser3");
    lostMessages->runForUser("TestUser3");
    CBL_ASSERT(m_wiki->readPageContent("Discussion utilisateur:TestMentor").ends_with(cbl::unindent(R"(
        == Message de TestUser3 ==
        Bonjour TestMentor,

        [[Utilisateur:TestUser3|TestUser3]] a reçu un message de bienvenue signé par vous et y a répondu, mais sans vous notifier. Vous pouvez lire son message sur sa [[Discussion utilisateur:TestUser3|page de discussion]] ([[Spécial:Diff/12|voir le diff]]).

        J'ai détecté que le message est du type « remerciements ». À l'avenir, pour ne pas vous solliciter trop souvent, je ne vous signalerai '''que les questions''', sauf si vous indiquez vouloir recevoir aussi les messages de remerciement (voir les instructions sur [[Utilisateur:OrlodrimBot/Messages sans destinataire|cette page]]).

        ~~~~)")));
    CBL_ASSERT_EQ(cbl::readFile(mentorStateFile.path()), "TestMentor|anything\nTestMentor|thanks\n");

    lostMessages =
        make_unique<LostMessages>(m_wiki.get(), mentorStateFile.path(), make_unique<MockMessageClassifier>());
    m_wiki->setPageContent("Discussion utilisateur:TestUser4",
                           "Bienvenue sur Wikipédia, TestUser4. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    m_wiki->setPageContent("Discussion utilisateur:TestUser4",
                           "Bienvenue sur Wikipédia, TestUser4. [[Utilisateur:TestMentor|TestMentor]]\n"
                           ":Merci ! (llmcat=Thanks) [[Utilisateur:TestUser4|TestUser4]]",
                           "TestUser4");
    lostMessages->runForUser("TestUser4");
    CBL_ASSERT(m_wiki->readPageContent("Discussion utilisateur:TestMentor").find("TestUser4") == string::npos);

    m_wiki->setPageContent("Discussion utilisateur:TestMentor", "{{bots|optin=notif-réponse-bienvenue-extra}}");
    lostMessages =
        make_unique<LostMessages>(m_wiki.get(), mentorStateFile.path(), make_unique<MockMessageClassifier>());
    m_wiki->setPageContent("Discussion utilisateur:TestUser5",
                           "Bienvenue sur Wikipédia, TestUser5. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    m_wiki->setPageContent("Discussion utilisateur:TestUser5",
                           "Bienvenue sur Wikipédia, TestUser5. [[Utilisateur:TestMentor|TestMentor]]\n"
                           ":Merci ! (llmcat=Thanks) [[Utilisateur:TestUser5|TestUser5]]",
                           "TestUser5");
    lostMessages->runForUser("TestUser5");
    CBL_ASSERT(m_wiki->readPageContent("Discussion utilisateur:TestMentor").find("TestUser5") != string::npos);
  }
  CBL_TEST_CASE(QuestionAfterWelcomeMessage_MentorOptout) {
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    postMessage(":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_wiki->setPageContent("Discussion utilisateur:TestMentor", "{{bots|optout=notif-réponse-bienvenue}}",
                           "TestMentor");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPage("Discussion utilisateur:TestMentor", mwc::RP_USER).user, "TestMentor");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
        Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]
        :Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]
        ::Bonjour TestUser,
        ::Je suis un robot de Wikipédia. J'ai remarqué que vous avez répondu au message d'accueil sur votre propre page de discussion.
        ::Comme le message d'accueil est automatique, TestMentor ne sera pas averti de votre réponse. N'hésitez pas à {{subst:gender:TestMentor|le|la}} contacter sur [[Discussion utilisateur:TestMentor|sa page de discussion]].
        ::~~~~)"));
  }
  CBL_TEST_CASE(QuestionAfterWelcomeMessage_MentorTalkPageIsRedirect) {
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    postMessage(":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_wiki->setPageContent("Discussion utilisateur:TestMentor", "#redirect[[User talk:TestMentor2]]", "TestMentor");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPage("Discussion utilisateur:TestMentor", mwc::RP_USER).user, "TestMentor");
  }
  CBL_TEST_CASE(QuestionAfterWelcomeMessage_MentorIsInactive) {
    m_wiki->inactiveUsers["TestMentor"] = "inactive";
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    postMessage(":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_wiki->setPageContent("Discussion utilisateur:TestMentor", "-", "TestMentor");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPage("Discussion utilisateur:TestMentor", mwc::RP_USER).user, "TestMentor");
  }
  CBL_TEST_CASE(QuestionAfterWelcomeMessage_MentorIsNotAValidUser) {
    m_wiki->inactiveUsers["TestMentor"] = "missing";
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    postMessage(":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_wiki->setPageContent("Discussion utilisateur:TestMentor", "-", "TestMentor");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPage("Discussion utilisateur:TestMentor", mwc::RP_USER).user, "TestMentor");
  }
  CBL_TEST_CASE(QuestionAfterSalebotDeletion) {
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    postMessage(
        "== Annonce de suppression de page ==\n"
        "[[Utilisateur:Salebot|Salebot]]",
        "Salebot");
    postMessage(":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
        Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]
        == Annonce de suppression de page ==
        [[Utilisateur:Salebot|Salebot]]
        :Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]
        ::Bonjour TestUser,
        ::Je suis un robot de Wikipédia. J'ai remarqué que vous avez répondu à un message envoyé par un autre robot. Nous les robots ne pouvons pas comprendre les messages qui nous sont écrits ! '''Aucun humain ne sera prévenu de votre message''' si vous ne le postez pas à l'endroit approprié.
        ::Si vous voulez contester la suppression de la page, faites une demande sur [[Wikipédia:Demande de restauration de page]]. Pour les questions plus générales, vous pouvez vous adresser au [[Wikipédia:Forum des nouveaux|forum des nouveaux]] ou à [[Discussion utilisateur:TestMentor|TestMentor]], qui vous a été {{subst:gender:TestMentor|assigné|assignée}} comme mentor.
        ::~~~~)"));
  }
  CBL_TEST_CASE(QuestionAfterSalebotRevert) {
    postMessage(
        "== Salebot a annulé votre modification sur X ==\n"
        "[[Utilisateur:Salebot|Salebot]]",
        "Salebot");
    postMessage(":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
        == Salebot a annulé votre modification sur X ==
        [[Utilisateur:Salebot|Salebot]]
        :Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]
        ::Bonjour TestUser,
        ::Je suis un robot de Wikipédia. J'ai remarqué que vous avez répondu à un message envoyé par un autre robot. Nous les robots ne pouvons pas comprendre les messages qui nous sont écrits ! '''Aucun humain ne sera prévenu de votre message''' si vous ne le postez pas à l'endroit approprié.
        ::Vous pouvez par exemple vous adresser au [[Wikipédia:Forum des nouveaux|forum des nouveaux]].
        ::~~~~)"));
  }
  CBL_TEST_CASE(QuestionAfterSalebotRevertForRecreatedPage) {
    postMessage(
        "== Salebot a annulé votre modification sur X ==\n"
        "Ne recréez pas cette page vous-même. [[Utilisateur:Salebot|Salebot]]",
        "Salebot");
    postMessage(":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
        == Salebot a annulé votre modification sur X ==
        Ne recréez pas cette page vous-même. [[Utilisateur:Salebot|Salebot]]
        :Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]
        ::Bonjour TestUser,
        ::Je suis un robot de Wikipédia. J'ai remarqué que vous avez répondu à un message envoyé par un autre robot. Nous les robots ne pouvons pas comprendre les messages qui nous sont écrits ! '''Aucun humain ne sera prévenu de votre message''' si vous ne le postez pas à l'endroit approprié.
        ::Comme la page a déjà été supprimée une première fois, vous ne pourrez pas intervenir dessus pour l'instant. Si vous souhaitez que la version originale soit restaurée ou que la nouvelle soit conservée, faites une demande sur [[Wikipédia:Demande de restauration de page]]. Pour les questions plus générales, vous pouvez vous adresser au [[Wikipédia:Forum des nouveaux|forum des nouveaux]].
        ::~~~~)"));
  }
  CBL_TEST_CASE(QuestionAfterNaggoBotMessage) {
    postMessage(
        "== Concernant votre demande de restauration de la page X ==\n"
        "[[Utilisateur:NaggoBot|NaggoBot]]",
        "NaggoBot");
    postMessage(":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
        == Concernant votre demande de restauration de la page X ==
        [[Utilisateur:NaggoBot|NaggoBot]]
        :Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]
        ::Bonjour TestUser,
        ::Je suis un robot de Wikipédia. J'ai remarqué que vous avez répondu à un message envoyé par un autre robot. Nous les robots ne pouvons pas comprendre les messages qui nous sont écrits ! '''Aucun humain ne sera prévenu de votre message''' si vous ne le postez pas à l'endroit approprié.
        ::Vous pouvez par exemple vous adresser au [[Wikipédia:Forum des nouveaux|forum des nouveaux]].
        ::~~~~)"));
  }
  CBL_TEST_CASE(QuestionAfterOrlodrimBotMessage) {
    postMessage(
        "== Article « X » transformé en brouillon ==\n"
        "[[Utilisateur:OrlodrimBot|OrlodrimBot]]",
        "OrlodrimBot");
    postMessage(":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
        == Article « X » transformé en brouillon ==
        [[Utilisateur:OrlodrimBot|OrlodrimBot]]
        :Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]
        ::Désolé, je suis un robot et je ne suis pas capable de comprendre les messages qui me sont écrits ! '''Aucun humain ne sera prévenu de votre message''' si vous ne le postez pas à l'endroit approprié.
        ::Je vous conseille de contacter le contributeur indiqué dans mon message précédent (cliquez sur son nom) ou le [[Wikipédia:Forum des nouveaux|forum des nouveaux]].
        ::~~~~)"));
  }
  /*
  CBL_TEST_CASE(QuestionInSeparateSection) {
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    postMessage(
        "\n== Question ==\n"
        "Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]",
        "TestUser");
    m_wiki->setPageContent("Discussion utilisateur:TestMentor", "-", "TestMentor");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
      Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]

      == Question ==
      Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]
      :Bonjour TestUser,
      :Je suis un robot de Wikipédia. J'ai remarqué que vous avez écrit sur votre propre page de discussion alors que
  celle-ci ne contient qu'un message d'accueil. En procédant ainsi, '''aucun humain ne sera prévenu de votre message'''.
  Pour obtenir une réponse, postez votre message sur une page de discussion communautaire ou sur celle d'un autre
  utilisateur. :Vous pouvez par exemple vous adresser au [[Wikipédia:Forum des nouveaux|forum des nouveaux]] ou à
  [[Discussion utilisateur:TestMentor|TestMentor]], qui vous a été {{subst:gender:TestMentor|assigné|assignée}} comme
  mentor.
      :~~~~)"));
  }
  */
  CBL_TEST_CASE(QuestionInSeparateSectionButWithMentorName) {
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    postMessage(
        "\n== Question ==\n"
        "Quoi Testmentor ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]",
        "TestUser");
    m_wiki->setPageContent("Discussion utilisateur:TestMentor", "-", "TestMentor");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPage("Discussion utilisateur:TestUser", mwc::RP_USER).user, "TestUser");
    CBL_ASSERT(m_wiki->readPageContent("Discussion utilisateur:TestMentor").find("TestUser") != string::npos);
  }
  CBL_TEST_CASE(ThanksInSeparateSection) {
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    postMessage(
        "\n== Merci ==\n"
        "Merci ! (llmcat=Thanks) [[Utilisateur:TestUser|TestUser]]",
        "TestUser");
    m_wiki->setPageContent("Discussion utilisateur:TestMentor", "-", "TestMentor");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestMentor"), cbl::unindent(R"(
      -

      == Message de TestUser ==
      Bonjour TestMentor,

      [[Utilisateur:TestUser|TestUser]] a reçu un message de bienvenue signé par vous et y a répondu, mais sans vous notifier. Vous pouvez lire son message sur sa [[Discussion utilisateur:TestUser|page de discussion]] ([[Spécial:Diff/3|voir le diff]]).

      Comme c'est la première fois que je vous envoie une telle notification, voici quelques informations complémentaires :
      * Je suis programmé pour détecter les messages laissés par les nouveaux utilisateurs sur leur page de discussion dont personne ne serait averti autrement.
      * Je les transmets à leur mentor en fonction de certains critères, expliqués sur [[Utilisateur:OrlodrimBot/Messages sans destinataire|cette page]].
      * Pour ne pas vous solliciter trop souvent, je ne vous signalerai à l'avenir '''que les questions''' et non les messages de remerciement comme celui-ci, sauf si vous indiquez explicitement que vous voulez recevoir les deux (voir la [[Utilisateur:OrlodrimBot/Messages sans destinataire|même page]]).
      * Pour vous désabonner entièrement de ces notifications, ajoutez {{((}}bots|optout=notif-réponse-bienvenue{{))}} dans l'en-tête de votre page de discussion.

      ~~~~)"));
  }
  CBL_TEST_CASE(QuestionOnNewPage) {
    postMessage(":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
      :Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]
      ::Bonjour TestUser,
      ::Je suis un robot de Wikipédia. J'ai remarqué que vous avez écrit sur votre propre page de discussion alors qu'aucun message ne vous a été envoyé. En procédant ainsi, '''aucun humain ne sera prévenu de votre message'''. Pour obtenir une réponse, postez votre message sur une page de discussion communautaire ou sur celle d'un autre utilisateur.
      ::Vous pouvez par exemple vous adresser au [[Wikipédia:Forum des nouveaux|forum des nouveaux]].
      ::~~~~)"));
  }
  CBL_TEST_CASE(QuestionOnNewPageInEnglish) {
    postMessage(":Quoi ? (llmcat=WikiQuestion lang=en) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
      :Quoi ? (llmcat=WikiQuestion lang=en) [[Utilisateur:TestUser|TestUser]]
      ::Bonjour TestUser,
      ::Je suis un robot de Wikipédia. J'ai remarqué que vous avez écrit sur votre propre page de discussion alors qu'aucun message ne vous a été envoyé. En procédant ainsi, '''aucun humain ne sera prévenu de votre message'''. Pour obtenir une réponse, postez votre message sur une page de discussion communautaire ou sur celle d'un autre utilisateur.
      ::Vous pouvez par exemple vous adresser au [[Wikipédia:Forum des nouveaux|forum des nouveaux]].
      ::{{GBR-d}} This is an automated response. It sounds like your message is written in English, but you are on the French version of Wikipedia. Did you intend to visit the [[:en:|English version]]? If your concern is about the French Wikipedia but you are not fluent in French, ask on [[Wikipédia:Bistro des non-francophones]].
      ::~~~~)"));
  }
  CBL_TEST_CASE(QuestionOnNewPageBlockedUser) {
    postMessage(":Pourriez-vous me débloquer ? (llmcat=WikiQuestion blocked=true) [[Utilisateur:TestUser|TestUser]]",
                "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
      :Pourriez-vous me débloquer ? (llmcat=WikiQuestion blocked=true) [[Utilisateur:TestUser|TestUser]]
      ::Bonjour TestUser,
      ::Je suis un robot de Wikipédia. J'ai remarqué que vous avez écrit sur votre propre page de discussion alors qu'aucun message ne vous a été envoyé. En procédant ainsi, '''aucun humain ne sera prévenu de votre message'''. Pour obtenir une réponse, postez votre message sur une page de discussion communautaire ou sur celle d'un autre utilisateur.
      ::Vous pouvez par exemple vous adresser au [[Wikipédia:Forum des nouveaux|forum des nouveaux]].
      ::Si un blocage vous empêche d'écrire ailleurs que sur cette page, vous pouvez demander un déblocage en écrivant ici un message contenant <code><nowiki>{{Déblocage}}</nowiki></code>. La demande sera transmise aux administrateurs.
      ::~~~~)"));
  }
  CBL_TEST_CASE(QuestionOnNewPageInEnglishBlockedUser) {
    postMessage(
        ":Could you please unblock me ? (llmcat=WikiQuestion lang=en blocked=true) [[Utilisateur:TestUser|TestUser]]",
        "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
      :Could you please unblock me ? (llmcat=WikiQuestion lang=en blocked=true) [[Utilisateur:TestUser|TestUser]]
      ::Bonjour TestUser,
      ::Je suis un robot de Wikipédia. J'ai remarqué que vous avez écrit sur votre propre page de discussion alors qu'aucun message ne vous a été envoyé. En procédant ainsi, '''aucun humain ne sera prévenu de votre message'''. Pour obtenir une réponse, postez votre message sur une page de discussion communautaire ou sur celle d'un autre utilisateur.
      ::Vous pouvez par exemple vous adresser au [[Wikipédia:Forum des nouveaux|forum des nouveaux]].
      ::Si un blocage vous empêche d'écrire ailleurs que sur cette page, vous pouvez demander un déblocage en écrivant ici un message contenant <code><nowiki>{{Déblocage}}</nowiki></code>. La demande sera transmise aux administrateurs.
      ::{{GBR-d}} This is an automated response. It sounds like your message is written in English, but you are on the French version of Wikipedia. Did you intend to visit the [[:en:|English version]]? If your concern is about the French Wikipedia but you are not fluent in French, ask on [[Wikipédia:Bistro des non-francophones]].
      ::If you cannot write on other pages due to a block, you may request to be unblocked by writing a message containing <code><nowiki>{{Déblocage}}</nowiki></code> on this page. Your request will be forwarded to administrators.
      ::~~~~)"));
  }
  CBL_TEST_CASE(DifferentIndentation) {
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    postMessage("Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
        Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]
        Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]
        :Bonjour TestUser,
        :Je suis un robot de Wikipédia. J'ai remarqué que vous avez répondu au message d'accueil sur votre propre page de discussion.
        :Comme le message d'accueil est automatique, TestMentor ne sera pas averti de votre réponse. N'hésitez pas à {{subst:gender:TestMentor|le|la}} contacter sur [[Discussion utilisateur:TestMentor|sa page de discussion]].
        :~~~~)"));
  }
  CBL_TEST_CASE(QuestionNotAtEndOfPage) {
    postMessage("Message. [[Utilisateur:NaggoBot|NaggoBot]]", "NaggoBot");
    postMessage(
        "== Annonce de suppression de page ==\n"
        "[[Utilisateur:Salebot|Salebot]]\n",
        "Salebot");
    m_wiki->setPageContent("Discussion utilisateur:TestUser",
                           "Message. [[Utilisateur:NaggoBot|NaggoBot]]\n"
                           ":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]\n"
                           "== Annonce de suppression de page ==\n"
                           "[[Utilisateur:Salebot|Salebot]]\n",
                           "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
      Message. [[Utilisateur:NaggoBot|NaggoBot]]
      :Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]
      ::Bonjour TestUser,
      ::Je suis un robot de Wikipédia. J'ai remarqué que vous avez écrit sur votre propre page de discussion alors que celle-ci ne contient que des messages de robots. Nous les robots ne pouvons pas comprendre les messages qui nous sont écrits ! '''Aucun humain ne sera prévenu de votre message''' si vous ne le postez pas à l'endroit approprié.
      ::Vous pouvez par exemple vous adresser au [[Wikipédia:Forum des nouveaux|forum des nouveaux]].
      ::~~~~

      == Annonce de suppression de page ==
      [[Utilisateur:Salebot|Salebot]])"));
  }
  CBL_TEST_CASE(QuestionNotAtEndOfPage2) {
    postMessage("Message. [[Utilisateur:NaggoBot|NaggoBot]]", "NaggoBot");
    postMessage(
        "\n== Annonce de suppression de page ==\n"
        "[[Utilisateur:Salebot|Salebot]]\n",
        "Salebot");
    m_wiki->setPageContent("Discussion utilisateur:TestUser",
                           "Message. [[Utilisateur:NaggoBot|NaggoBot]]\n\n"
                           ":Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]\n"
                           "== Annonce de suppression de page ==\n"
                           "[[Utilisateur:Salebot|Salebot]]\n",
                           "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent("Discussion utilisateur:TestUser"), cbl::unindent(R"(
      Message. [[Utilisateur:NaggoBot|NaggoBot]]

      :Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]
      ::Bonjour TestUser,
      ::Je suis un robot de Wikipédia. J'ai remarqué que vous avez écrit sur votre propre page de discussion alors que celle-ci ne contient que des messages de robots. Nous les robots ne pouvons pas comprendre les messages qui nous sont écrits ! '''Aucun humain ne sera prévenu de votre message''' si vous ne le postez pas à l'endroit approprié.
      ::Vous pouvez par exemple vous adresser au [[Wikipédia:Forum des nouveaux|forum des nouveaux]].
      ::~~~~

      == Annonce de suppression de page ==
      [[Utilisateur:Salebot|Salebot]])"));
  }
  CBL_TEST_CASE(MultipleBots) {
    m_wiki->setPageContent("Discussion utilisateur:TestUser", "Bienvenue. [[Utilisateur:TestMentor|TestMentor]]",
                           "Loveless");
    m_wiki->setPageContent("Discussion utilisateur:TestUser", "Bienvenue ! [[Utilisateur:TestMentor|TestMentor]]",
                           "Salebot");
    m_wiki->setPageContent(
        "Discussion utilisateur:TestUser",
        "Bienvenue ! [[Utilisateur:TestMentor|TestMentor]]\n:Salut ! (llmcat=Other) [[Utilisateur:TestUser|TestUser]]",
        "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPageContent(LIST_PAGE), cbl::unindent(R"(
        <!-- BEGIN BOT SECTION -->
        * 2 janvier 2025 à 04:04 : {{u|TestUser}} a [[Spécial:Diff/4|modifié]] sa page de discussion (catégorisation : modèle local = autre / LLM = autre)
        <!-- END BOT SECTION -->)"));
  }
  CBL_TEST_CASE(UnblockRequest) {
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    postMessage("{{Déblocage|llmcat=WikiQuestion}}", "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPage("Discussion utilisateur:TestUser", mwc::RP_USER).user, "TestUser");
  }
  CBL_TEST_CASE(MessageWithNotification) {
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    postMessage("{{Notif|OtherUser}} llmcat=WikiQuestion", "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPage("Discussion utilisateur:TestUser", mwc::RP_USER).user, "TestUser");
  }
  CBL_TEST_CASE(MessageWithNotificationReplacingOtherTemplate) {
    postMessage("{{Some template}}", "Loveless");
    m_wiki->setPageContent("Discussion utilisateur:TestUser", "{{Notif}} llmcat=WikiQuestion {{Some template}}",
                           "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPage("Discussion utilisateur:TestUser", mwc::RP_USER).user, "TestUser");
  }
  CBL_TEST_CASE(MessageWithSimpleNotification) {
    postMessage("Bienvenue sur Wikipédia, TestUser. [[Utilisateur:TestMentor|TestMentor]]", "Loveless");
    postMessage("[[Utilisateur:OtherUser]] llmcat=WikiQuestion", "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPage("Discussion utilisateur:TestUser", mwc::RP_USER).user, "TestUser");
  }
  CBL_TEST_CASE(TalkPageCreationWithTemplate) {
    postMessage("{{Whatever|llmcat=WikiQuestion}}", "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPage("Discussion utilisateur:TestUser", mwc::RP_USER).user, "TestUser");
  }
  CBL_TEST_CASE(ReplyOnce) {
    // postMessage("Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_wiki->setPageContent("Discussion utilisateur:TestUser", ":Réponse [[Utilisateur:OrlodrimBot|OrlodrimBot]]",
                           "OrlodrimBot", "Réponse automatique");
    postMessage("Quoi ? (llmcat=WikiQuestion) [[Utilisateur:TestUser|TestUser]]", "TestUser");
    m_lostMessages->runForUser("TestUser");
    CBL_ASSERT_EQ(m_wiki->readPage("Discussion utilisateur:TestUser", mwc::RP_USER).user, "TestUser");
  }

  unique_ptr<CustomMockWiki> m_wiki;
  unique_ptr<LostMessages> m_lostMessages;
  cbl::TempFile m_mentorStateFile;
};

int main() {
  LostMessagesTest().run();
  return 0;
}
