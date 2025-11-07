#include "lost_messages_lib.h"
#include <re2/re2.h>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "cbl/file.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/bot_exclusion.h"
#include "mwclient/parser.h"
#include "mwclient/titles_util.h"
#include "mwclient/util/bot_section.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"
#include "orlodrimbot/wikiutil/date_formatter.h"
#include "orlodrimbot/wikiutil/wiki_local_time.h"
#include "message_classifier.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::NS_USER;
using mwc::OLDEST_FIRST;
using mwc::revid_t;
using mwc::Revision;
using mwc::Wiki;
using std::make_unique;
using std::optional;
using std::pair;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;
using wikiutil::DateFormatter;

namespace {

unordered_set<string_view> NOTIF_TEMPLATES = {
    "Bonjour",      "Bonsoir",       "Mention",        "Merci",           "N+",    "N-",     "N--",
    "Ni",           "Notif discret", "Notif discrète", "Notif invisible", "Notif", "Notif-", "Notifd",
    "Notification", "Notifinv",      "Ping",           "Ping-",
};

// Not notification templates, but they tend to cause the message to be detected as a question whereas they are not
// intended to be used in discusions (they indicated an experienced user creating some draft).
unordered_set<string_view> TEMPLATES_DISABLING_PROCESSING = {
    "Combien", "Comment", "En quoi", "Où", "Pourquoi", "Quand", "Qui", "Quoi",
};

constexpr string_view REPLY_COMMENT = "Réponse automatique";

// To prevent duplicate posting, the bot running the script should not be included in those lists.
const unordered_set<string_view> WELCOME_BOTS = {
    "Loveless",
    "Loveless bienvenue",
    "Message de bienvenue",
};

const unordered_set<string_view> OTHER_BOTS = {
    "KunMilanoRobot", "Flow talk page manager",    "NaggoBot", "OrlodrimBot",
    "Salebot",        "Signature manquante (bot)", "ZéroBot",
};

bool isBot(string_view user) {
  return WELCOME_BOTS.count(user) != 0 || OTHER_BOTS.count(user) != 0;
}

bool isAutoreplyFromThisScript(string_view user, string_view comment) {
  return user == "OrlodrimBot" && comment == REPLY_COMMENT;
}

string makeGreeting(string_view user, bool notify) {
  if (notify) {
    return cbl::concat("Bonjour {{notif-|", user, "}}");
  } else if (mwc::getAccountType(user) == mwc::AccountType::TEMP_USER) {
    return "Bonjour";
  } else {
    return cbl::concat("Bonjour ", user);
  }
}

pair<string_view, string_view> splitRootAndSubpage(string_view page) {
  size_t slashPosition = page.find('/');
  if (slashPosition == string::npos) {
    return {page, ""};
  } else {
    return {cbl::trim(page.substr(0, slashPosition), cbl::TRIM_RIGHT), page.substr(slashPosition)};
  }
}

void processRecentChanges(Wiki& wiki, live_replication::RecentChangesReader& recentChangesReader,
                          const DateDiff& maxAge, vector<RevisionToCheck>* revisionsToCheck,
                          unordered_set<string>* pagesToIgnore, string* rcToken) {
  unordered_set<string> pagesToIgnoreInternal;
  recentChangesReader.enumRecentChanges(
      {.type = mwc::RC_EDIT | mwc::RC_NEW,
       .properties = mwc::RP_TITLE | mwc::RP_USER | mwc::RP_TIMESTAMP | mwc::RP_REVID | mwc::RP_COMMENT,
       .start = cbl::Date::now() - maxAge,
       .continueToken = rcToken},
      [&](const mwc::RecentChange& rc) {
        mwc::TitleParts titleParts = wiki.parseTitle(rc.title());
        if (titleParts.namespaceNumber != mwc::NS_USER_TALK) return;
        string_view unprefixedTitle = titleParts.unprefixedTitle();
        auto [pageOwner, subpage] = splitRootAndSubpage(unprefixedTitle);
        if (subpage != "" && subpage != "/Brouillon") return;
        if (pagesToIgnoreInternal.count(rc.title()) != 0) return;
        if (rc.user() == pageOwner) {
          if (revisionsToCheck != nullptr) {
            revisionsToCheck->push_back({.page = rc.title(), .user = string(pageOwner), .revid = rc.revision().revid});
          }
        } else if (!isBot(rc.user()) || isAutoreplyFromThisScript(rc.user(), rc.comment())) {
          pagesToIgnoreInternal.insert(rc.title());
        }
      });
  if (pagesToIgnore != nullptr) {
    *pagesToIgnore = std::move(pagesToIgnoreInternal);
  }
}

vector<RevisionToCheck> getRevisionsToCheck(Wiki& wiki, live_replication::RecentChangesReader& recentChangesReader,
                                            string& rcToken) {
  vector<RevisionToCheck> revisions;
  processRecentChanges(wiki, recentChangesReader, DateDiff::fromDays(1), &revisions, nullptr, &rcToken);
  return revisions;
}

unordered_set<string> enumPagesToIgnore(Wiki& wiki, live_replication::RecentChangesReader& recentChangesReader,
                                        const cbl::DateDiff& maxAge) {
  unordered_set<string> pagesToIgnore;
  processRecentChanges(wiki, recentChangesReader, DateDiff::fromHours(6), nullptr, &pagesToIgnore, nullptr);
  return pagesToIgnore;
}

pair<size_t, size_t> extractDiff(string_view oldText, string_view newText) {
  size_t size1 = oldText.size(), size2 = newText.size();
  if (size1 >= size2) {
    return {0, 0};
  }
  size_t diffStart = 0;
  for (; diffStart < size1 && oldText[diffStart] == newText[diffStart]; diffStart++) {}
  size_t diffEnd = diffStart + size2 - size1;
  if (newText.substr(diffEnd) != oldText.substr(diffStart)) {
    return {0, 0};
  }
  // Sometimes, the diff position is ambiguous, e.g. replacing "{{a}}" with "{{b}} {{a}}" can be interpreted as
  // adding "{{b}} " or by adding "b}} {{". Try to keep full templates.
  // See also the test case MessageWithNotificationReplacingOtherTemplate.
  while (diffStart > 0 && newText[diffStart - 1] == newText[diffEnd - 1]) {
    char c = newText[diffStart - 1];
    if (c != '{' && c != '[' && c != '(') break;
    diffStart--;
    diffEnd--;
  }
  return {diffStart, diffEnd};
}

string composeMessage(const Post& post, const PostAnalysis& postAnalysis) {
  string contacts = "au [[Wikipédia:Forum des nouveaux|forum des nouveaux]]";
  if (!postAnalysis.mentor.empty()) {
    cbl::append(contacts, " ou à [[Discussion utilisateur:", postAnalysis.mentor, "|", postAnalysis.mentor,
                "]], qui vous a été {{subst:gender:", postAnalysis.mentor, "|assigné|assignée}} comme mentor");
  }
  string message = cbl::concat(makeGreeting(post.user, postAnalysis.onDraftTalk), ",\nJe suis un robot de Wikipédia. ");
  if (postAnalysis.onDraftTalk) {
    cbl::append(message,
                "J'ai remarqué que vous avez écrit sur la page de discussion de votre propre brouillon.\n"
                "En procédant ainsi, '''aucun humain ne sera prévenu de votre message'''. Pour obtenir une réponse, "
                "postez votre message sur une page de discussion communautaire ou sur celle d'un autre utilisateur.\n",
                "Par exemple, pour demander une relecture de votre brouillon, postez une demande sur la page "
                "[[Wikipédia:Forum de relecture]]. Pour les questions plus générales, vous pouvez vous adresser ",
                contacts, ".\n");
  } else if (postAnalysis.sectionType == SectionType::WELCOME_MESSAGE) {
    cbl::append(message,
                "J'ai remarqué que vous avez répondu au message d'accueil sur votre propre page de discussion.\n"
                "Comme le message d'accueil est automatique, ",
                postAnalysis.mentor,
                " ne sera pas averti de votre réponse. N'hésitez pas à {{subst:gender:", postAnalysis.mentor,
                "|le|la}} contacter sur [[Discussion utilisateur:", postAnalysis.mentor, "|sa page de discussion]].\n");
  } else if (postAnalysis.sectionType == SectionType::ORLODRIMBOT_CONVERTED_TO_DRAFT) {
    message =
        "Désolé, je suis un robot et je ne suis pas capable de comprendre les messages qui me sont écrits ! "
        "'''Aucun humain ne sera prévenu de votre message''' si vous ne le postez pas à l'endroit approprié.\n"
        "Je vous conseille de contacter le contributeur indiqué dans mon message précédent (cliquez sur son nom) "
        "ou le [[Wikipédia:Forum des nouveaux|forum des nouveaux]].\n";
  } else if (post.hasNonWelcomeBotMessage) {
    if (postAnalysis.sectionType == SectionType::SALEBOT_DELETION_MESSAGE ||
        postAnalysis.sectionType == SectionType::SALEBOT_POST_DELETION_MESSAGE ||
        postAnalysis.sectionType == SectionType::SALEBOT_REVERT_MESSAGE ||
        postAnalysis.sectionType == SectionType::NAGGOBOT_UNDELETE_REQUEST_MESSAGE) {
      message += "J'ai remarqué que vous avez répondu à un message envoyé par un autre robot. ";
    } else {
      message +=
          "J'ai remarqué que vous avez écrit sur votre propre page de discussion alors que celle-ci ne contient que "
          "des messages de robots. ";
    }
    message +=
        "Nous les robots ne pouvons pas comprendre les messages qui nous sont écrits ! '''Aucun humain ne sera prévenu "
        "de votre message''' si vous ne le postez pas à l'endroit approprié.\n";
    if (postAnalysis.sectionType == SectionType::SALEBOT_DELETION_MESSAGE) {
      cbl::append(message,
                  "Si vous voulez contester la suppression de la page, faites une demande sur "
                  "[[Wikipédia:Demande de restauration de page]]. Pour les questions plus générales, vous pouvez vous "
                  "adresser ",
                  contacts, ".\n");
    } else if (postAnalysis.sectionType == SectionType::SALEBOT_POST_DELETION_MESSAGE) {
      cbl::append(message,
                  "Comme la page a déjà été supprimée une première fois, vous ne pourrez pas intervenir dessus pour "
                  "l'instant. Si vous souhaitez que la version originale soit restaurée ou que la nouvelle soit "
                  "conservée, faites une demande sur [[Wikipédia:Demande de restauration de page]]. Pour les questions "
                  "plus générales, vous pouvez vous adresser ",
                  contacts, ".\n");
    } else {
      cbl::append(message, "Vous pouvez par exemple vous adresser ", contacts, ".\n");
    }
  } else {
    if (post.welcomeRevid != mwc::INVALID_REVID) {
      message +=
          "J'ai remarqué que vous avez écrit sur votre propre page de discussion alors que celle-ci ne contient qu'un "
          "message d'accueil. ";
    } else {
      message +=
          "J'ai remarqué que vous avez écrit sur votre propre page de discussion alors qu'aucun message ne vous a été "
          "envoyé. ";
    }
    cbl::append(message,
                "En procédant ainsi, '''aucun humain ne sera prévenu de votre message'''. Pour obtenir une réponse, "
                "postez votre message sur une page de discussion communautaire ou sur celle d'un autre utilisateur.\n",
                "Vous pouvez par exemple vous adresser ", contacts, ".\n");
  }
  bool mayBeBlocked = postAnalysis.classification.llmBlocked && post.page.find('/') == string::npos;
  if (mayBeBlocked) {
    message +=
        "Si un blocage vous empêche d'écrire ailleurs que sur cette page, vous pouvez demander un déblocage en "
        "écrivant ici un message contenant <code><nowiki>{{Déblocage}}</nowiki></code>. La demande sera "
        "transmise aux administrateurs.\n";
  }
  if (postAnalysis.classification.llmLanguage == MessageClassification::Language::ENGLISH) {
    message +=
        "{{GBR-d}} This is an automated response. It sounds like your message is written in English, but you "
        "are on the French version of Wikipedia. Did you intend to visit the [[:en:|English version]]? If "
        "your concern is about the French Wikipedia but you are not fluent in French, ask on "
        "[[Wikipédia:Bistro des non-francophones]].\n";
    if (mayBeBlocked) {
      message +=
          "If you cannot write on other pages due to a block, you may request to be unblocked by writing a "
          "message containing <code><nowiki>{{Déblocage}}</nowiki></code> on this page. Your request will be "
          "forwarded to administrators.\n";
    }
  }
  message += "~~~~\n";

  int indentation = postAnalysis.messageIndentation <= 2 ? postAnalysis.messageIndentation + 1 : 1;
  string indentationStr(indentation, ':');
  string indentedMessage;
  for (string_view line : cbl::splitLines(message)) {
    cbl::append(indentedMessage, indentationStr, line, "\n");
  }

  return indentedMessage;
}

string extractPostDiff(const Post& post) {
  if (post.numEdits >= 2) {
    return cbl::concat("Spécial:Diff/", std::to_string(post.previousRevid), "/", std::to_string(post.revid));
  } else {
    return "Spécial:Diff/" + std::to_string(post.revid);
  }
}

string composeMentorMessage(const Post& post, const PostAnalysis& postAnalysis, const MentorState& mentorState,
                            bool forThanks) {
  string message =
      cbl::concat(makeGreeting(postAnalysis.mentor, false), ",\n\n", "[[Utilisateur:", post.user, "|", post.user,
                  "]] a reçu un message de bienvenue signé par vous et y a répondu, mais sans vous notifier. "
                  "Vous pouvez lire son message sur sa [[Discussion utilisateur:",
                  post.user, "|page de discussion]] ([[", extractPostDiff(post), "|voir le diff]]).");
  if (!mentorState.anythingForwarded) {
    message +=
        "\n\nComme c'est la première fois que je vous envoie une telle notification, voici quelques informations "
        "complémentaires :\n"
        "* Je suis programmé pour détecter les messages laissés par les nouveaux utilisateurs sur leur page de "
        "discussion dont personne ne serait averti autrement.\n"
        "* Je les transmets à leur mentor en fonction de certains critères, expliqués sur "
        "[[Utilisateur:OrlodrimBot/Messages sans destinataire|cette page]].\n";
    if (forThanks) {
      message +=
          "* Pour ne pas vous solliciter trop souvent, je ne vous signalerai à l'avenir '''que les questions''' et non "
          "les messages de remerciement comme celui-ci, sauf si vous indiquez explicitement que vous voulez recevoir "
          "les deux (voir la [[Utilisateur:OrlodrimBot/Messages sans destinataire|même page]]).\n";
    }
    message +=
        "* Pour vous désabonner entièrement de ces notifications, ajoutez "
        "{{((}}bots|optout=notif-réponse-bienvenue{{))}} dans l'en-tête de votre page de discussion.";
  } else if (forThanks && !mentorState.thanksForwarded) {
    message +=
        "\n\nJ'ai détecté que le message est du type « remerciements ». À l'avenir, pour ne pas vous solliciter trop "
        "souvent, je ne vous signalerai '''que les questions''', sauf si vous indiquez vouloir recevoir aussi les "
        "messages de remerciement (voir les instructions sur "
        "[[Utilisateur:OrlodrimBot/Messages sans destinataire|cette page]]).";
  }
  return message;
}

}  // namespace

LostMessages::LostMessages(Wiki* wiki, const string& mentorStatesFile, unique_ptr<MessageClassifier> messageClassifier)
    : m_wiki(wiki), m_mentorStatesFile(mentorStatesFile), m_messageClassifier(std::move(messageClassifier)) {
  if (m_messageClassifier == nullptr) {
    m_messageClassifier = make_unique<MessageClassifier>();
    m_messageClassifier->setPrintThought(true);
  }
  if (!m_mentorStatesFile.empty()) {
    loadMentorStatesFile();
  }
}

void LostMessages::loadMentorStatesFile() {
  if (!cbl::fileExists(m_mentorStatesFile)) {
    return;
  }
  string content = cbl::readFile(m_mentorStatesFile);
  for (string_view line : cbl::splitLines(content)) {
    size_t pipePosition = line.find('|');
    if (pipePosition == string::npos) continue;
    string mentor(line.substr(0, pipePosition));
    string_view type = line.substr(pipePosition + 1);
    MentorState& mentorState = m_mentorStates[mentor];
    if (type == "anything") {
      mentorState.anythingForwarded = true;
    } else if (type == "thanks") {
      mentorState.thanksForwarded = true;
    }
  }
}

optional<Post> LostMessages::extractPostOfUser(const RevisionToCheck& revisionToCheck) {
  const string& title = revisionToCheck.page;
  const string& user = revisionToCheck.user;
  revid_t revid = revisionToCheck.revid;

  CBL_INFO << "Checking history of '" << title << "'";
  vector<mwc::Revision> revisions;
  constexpr int LONGEST_HISTORY_TO_CONSIDER = 20;
  try {
    revisions = m_wiki->getHistory({.title = title,
                                    .prop = mwc::RP_USER | mwc::RP_REVID | mwc::RP_TIMESTAMP | mwc::RP_COMMENT,
                                    .direction = OLDEST_FIRST,
                                    .limit = LONGEST_HISTORY_TO_CONSIDER + 1});
  } catch (const mwc::PageNotFoundError&) {
    CBL_INFO << "Page '" << title << "' does not exist";
    return std::nullopt;  // The page may already have been deleted due to a race condition.
  }
  if (revisions.empty()) {
    CBL_WARNING << "No revisions for page '" << title << "'";
    return std::nullopt;  // Sanity check, this shouldn't happen.
  }
  if (revisions.size() > LONGEST_HISTORY_TO_CONSIDER) {
    CBL_INFO << "Page '" << title << "' ignored because the history is too long";
    return std::nullopt;
  }
  if (revisions.back().user != user) {
    CBL_INFO << "Page '" << title << "' was last modified by '" << revisions.back().user
             << ", who is not the owner of the page";
    return std::nullopt;
  }
  const Revision& lastRevision = revisions.back();
  if (revid != mwc::INVALID_REVID && revid != lastRevision.revid) {
    CBL_INFO << "Page '" << title << "' last revid is " << lastRevision.revid << ", not " << revid;
    return std::nullopt;
  }
  Post post = {.page = title, .user = user, .timestamp = lastRevision.timestamp, .revid = lastRevision.revid};
  if (WELCOME_BOTS.count(revisions[0].user) != 0) {
    post.welcomeRevid = revisions[0].revid;
  }
  const Revision* previousRevision = nullptr;
  for (const mwc::Revision& revision : revisions) {
    if (revision.user == user) {
      if (previousRevision != nullptr &&
          (previousRevision->user != user ||
           revision.timestamp - previousRevision->timestamp > DateDiff::fromMinutes(5))) {
        post.previousRevid = previousRevision->revid;
        post.numEdits = 0;
      }
      post.numEdits++;
    } else if (isAutoreplyFromThisScript(revision.user, revision.comment)) {
      CBL_INFO << "Page '" << title << "' ignored because I already posted a reply there";
      return std::nullopt;
    } else if (OTHER_BOTS.count(revision.user) != 0) {
      post.hasNonWelcomeBotMessage = true;
    } else if (!isBot(revision.user)) {
      CBL_INFO << "Page '" << title << "' ignored because it was edited by '" << revision.user << "'";
      return std::nullopt;
    }
    previousRevision = &revision;
  }
  CBL_INFO << "The first non-bot contribution on '" << title
           << "' is by the owner of the page (timestamp=" << post.timestamp << ")";
  return post;
}

vector<Post> LostMessages::extractPostsFromRecentChanges(live_replication::RecentChangesReader& recentChangesReader,
                                                         json::Value& state) {
  string rcToken = state["rc_token"].str();
  vector<RevisionToCheck> revisionsToCheck = getRevisionsToCheck(*m_wiki, recentChangesReader, rcToken);
  state.getMutable("rc_token") = rcToken;

  vector<mwc::UserInfo> userInfos;
  userInfos.reserve(revisionsToCheck.size());
  for (const RevisionToCheck& revisionToCheck : revisionsToCheck) {
    userInfos.emplace_back().name = revisionToCheck.user;
  }
  vector<Post> posts;
  bool pagesToIgnoreInitialized = false;
  unordered_set<string> pagesToIgnore;
  m_wiki->getUsersInfo(mwc::UIP_EDIT_COUNT | mwc::UIP_GROUPS, userInfos);
  int userIndex = 0;
  for (const RevisionToCheck& revisionToCheck : revisionsToCheck) {
    mwc::UserInfo& userInfo = userInfos[userIndex++];
    if (userInfo.editCount > 50) {
      continue;
    }
    if (!pagesToIgnoreInitialized) {
      CBL_INFO << "Reading recent changes from the past 6 hours to check if the user should be ignored";
      pagesToIgnore = enumPagesToIgnore(*m_wiki, recentChangesReader, DateDiff::fromHours(6));
      pagesToIgnoreInitialized = true;
    }
    if (pagesToIgnore.count(revisionToCheck.page) == 0) {
      optional<Post> post = extractPostOfUser(revisionToCheck);
      if (post.has_value()) {
        posts.push_back(std::move(*post));
      } else {
        pagesToIgnore.insert(revisionToCheck.page);
      }
    }
  }
  return posts;
}

bool containsNotification(Wiki& wiki, const wikicode::List& parsedCode, const string& pageOwner) {
  for (const wikicode::Node& node : parsedCode.getNodes()) {
    switch (node.type()) {
      case wikicode::NT_TEMPLATE: {
        string templateName = wiki.normalizeTitle(node.asTemplate().name());
        if (NOTIF_TEMPLATES.count(templateName) != 0 || TEMPLATES_DISABLING_PROCESSING.count(templateName) != 0) {
          return true;
        }
        break;
      }
      case wikicode::NT_LINK: {
        mwc::TitleParts titleParts = wiki.parseTitle(node.asLink().target());
        auto [rootPage, subPage] = splitRootAndSubpage(titleParts.unprefixedTitle());
        if (titleParts.namespaceNumber == NS_USER && subPage.empty() && rootPage != pageOwner) {
          return true;
        }
        break;
      }
      default:
        break;
    }
  }
  return false;
}

bool containsUnblockTemplate(Wiki& wiki, const wikicode::List& parsedCode) {
  for (const wikicode::Template& template_ : parsedCode.getTemplates()) {
    if (wiki.normalizeTitle(template_.name()) == "Déblocage") {
      return true;
    }
  }
  return false;
}

bool LostMessages::extractPostContent(const Post& post, PostAnalysis& postAnalysis) {
  vector<Revision> revisions;
  revisions.reserve(3);
  revisions.emplace_back().revid = post.revid;
  if (post.previousRevid != mwc::INVALID_REVID) {
    revisions.emplace_back().revid = post.previousRevid;
    if (post.welcomeRevid != mwc::INVALID_REVID) {
      revisions.emplace_back().revid = post.welcomeRevid;
    }
  }
  m_wiki->readRevisions(mwc::RP_CONTENT, revisions);

  if (m_wiki->readRedirect(revisions[0].content, nullptr, nullptr)) {
    CBL_INFO << post.page << " was transformed into a redirect, ignoring";
    return false;
  }

  postAnalysis.onDraftTalk = post.page.ends_with("/Brouillon");
  postAnalysis.pageContent = revisions[0].content;
  auto [diffStart, diffEnd] = extractDiff(revisions.size() >= 2 ? revisions[1].content : "", postAnalysis.pageContent);
  if (diffStart == diffEnd) {
    CBL_INFO << "When editing '" << post.page << "' for the first time, the user did not only add content, ignoring";
    return false;
  }

  if (revisions.size() >= 3 && !postAnalysis.onDraftTalk) {
    static const re2::RE2 reUser(R"(\[\[(?i:Utilisateur|Utilisatrice|User):([^\[\]|]+)\|)");
    RE2::PartialMatch(revisions[2].content, reUser, &postAnalysis.mentor);
  }

  string_view contentView = postAnalysis.pageContent;
  postAnalysis.diff = contentView.substr(diffStart, diffEnd - diffStart);
  string_view sectionHeader;
  for (string_view line : cbl::splitLines(contentView.substr(0, diffStart))) {
    if (wikicode::getTitleLevel(line) != 0) {
      sectionHeader = line;
    }
  }
  size_t sectionStart = sectionHeader.empty() ? 0 : sectionHeader.data() - contentView.data();
  string_view section = contentView.substr(sectionStart, diffStart - sectionStart);
  if (cbl::trim(postAnalysis.diff).starts_with("==")) {
    // The message is in a separate section.
  } else if (sectionHeader.empty()) {
    if (section.find("Bienvenue sur Wikipédia,") != string::npos && !postAnalysis.mentor.empty()) {
      postAnalysis.sectionType = SectionType::WELCOME_MESSAGE;
    }
  } else {
    string sectionTitle = wikicode::getTitleContent(sectionHeader);
    if (section.find("|Salebot]]") != string::npos) {
      if (sectionTitle == "Annonce de suppression de page") {
        postAnalysis.sectionType = SectionType::SALEBOT_DELETION_MESSAGE;
      } else if (sectionTitle.find("Salebot a annulé votre modification") != string::npos) {
        if (section.find("Ne recréez pas cette page vous-même") != string::npos) {
          postAnalysis.sectionType = SectionType::SALEBOT_POST_DELETION_MESSAGE;
        } else {
          postAnalysis.sectionType = SectionType::SALEBOT_REVERT_MESSAGE;
        }
      }
    } else if (section.find("|NaggoBot]]") != string::npos &&
               sectionTitle.find("Concernant votre demande de restauration") != string::npos) {
      postAnalysis.sectionType = SectionType::NAGGOBOT_UNDELETE_REQUEST_MESSAGE;
    } else if (section.find("|OrlodrimBot]]") != string::npos &&
               sectionTitle.find("transformé en brouillon") != string::npos) {
      postAnalysis.sectionType = SectionType::ORLODRIMBOT_CONVERTED_TO_DRAFT;
    }
  }
  if (diffEnd == contentView.size() || (diffEnd > 0 && contentView[diffEnd - 1] == '\n')) {
    for (postAnalysis.answerStart = diffEnd; contentView.substr(0, postAnalysis.answerStart).ends_with("\n");
         postAnalysis.answerStart--) {}
    for (postAnalysis.answerEnd = postAnalysis.answerStart;
         contentView.substr(postAnalysis.answerEnd, 2).starts_with("\n\n"); postAnalysis.answerEnd++) {}
    int indentation = 0;
    for (string_view line : cbl::splitLines(postAnalysis.diff)) {
      if (!line.empty()) {
        for (indentation = 0; indentation < static_cast<int>(line.size()) && line[indentation] == ':'; indentation++) {}
      }
    }
    postAnalysis.messageIndentation = indentation;
  }

  wikicode::List parsedDiff = wikicode::parse(postAnalysis.diff);
  if (containsUnblockTemplate(*m_wiki, parsedDiff)) {
    CBL_INFO << "The message on '" << post.page << "' is an unblock request, ignoring";
    return false;
  } else if (containsNotification(*m_wiki, parsedDiff, post.user)) {
    CBL_INFO << "When editing '" << post.page << "' for the first time, the user notified another user, ignoring";
    return false;
  }
  return true;
}

bool LostMessages::analyzePost(const Post& post, PostAnalysis& postAnalysis) {
  if (!extractPostContent(post, postAnalysis)) {
    return false;
  }
  string normalizedDiff = string(postAnalysis.diff);
  if (!postAnalysis.mentor.empty()) {
    re2::RE2 mentorRegexp = "\\b(?i:" + RE2::QuoteMeta(postAnalysis.mentor) + ")\\b";
    RE2::GlobalReplace(&normalizedDiff, mentorRegexp, "monmentor");
  }
  postAnalysis.classification = m_messageClassifier->classify(normalizedDiff);
  return true;
}

string_view getFrenchCategoryName(MessageClassification::Category category) {
  switch (category) {
    case MessageClassification::Category::WIKI_QUESTION:
      return "question";
    case MessageClassification::Category::NON_WIKI_QUESTION:
      return "question non liée à Wikipédia";
    case MessageClassification::Category::THANKS:
      return "remerciements";
    case MessageClassification::Category::ARTICLE_DRAFT:
      return "brouillon d'article";
    case MessageClassification::Category::OTHER:
      return "autre";
    case MessageClassification::Category::UNKNOWN:
      break;
  }
  return "?";
}

const MentorState& LostMessages::getMentorState(const string& mentor) const {
  static constexpr MentorState DEFAULT_STATE;
  unordered_map<string, MentorState>::const_iterator mentorStateIt = m_mentorStates.find(mentor);
  return mentorStateIt != m_mentorStates.end() ? mentorStateIt->second : DEFAULT_STATE;
}

void LostMessages::setMentorState(const string& mentor, bool setAnythingForwarded, bool setThanksForwarded) {
  string extraLines;
  MentorState& mentorState = m_mentorStates[mentor];
  if (setAnythingForwarded && !mentorState.anythingForwarded) {
    mentorState.anythingForwarded = true;
    cbl::append(extraLines, mentor, "|anything\n");
  }
  if (setThanksForwarded && !mentorState.thanksForwarded) {
    mentorState.thanksForwarded = true;
    cbl::append(extraLines, mentor, "|thanks\n");
  }
  if (!extraLines.empty() && !m_mentorStatesFile.empty()) {
    FILE* file = fopen(m_mentorStatesFile.c_str(), "a");
    if (file != nullptr) {
      fprintf(file, "%s", extraLines.c_str());
      fclose(file);
    }
  }
}

bool LostMessages::hasOptedInToReceiveThanks(const string& mentor) const {
  string content;
  try {
    content = m_wiki->readPageContent("User talk:" + mentor);
  } catch (const mwc::PageNotFoundError&) {
    return false;
  }
  wikicode::List parsedContent = wikicode::parse(content);
  for (const wikicode::Template& template_ : parsedContent.getTemplates()) {
    if (m_wiki->normalizeTitle(template_.name()) == "Bots") {
      wikicode::ParsedFields parsedFields = template_.getParsedFields();
      string optin = parsedFields["optin"];
      if (optin.find("notif-réponse-bienvenue-extra") != string::npos) {
        return true;
      }
    }
  }
  return false;
}

bool LostMessages::postOnMentorTalkPage(const Post& post, const PostAnalysis& postAnalysis, string_view title,
                                        string_view messageBody, bool dryRun) {
  string mentorTalkPage = "User talk:" + postAnalysis.mentor;
  bool messageSentToMentor = false;
  try {
    m_wiki->editPage(mentorTalkPage, [&](string& content, string& summary) {
      messageSentToMentor = false;
      vector<Revision> lastContrib =
          m_wiki->getUserContribs({.user = postAnalysis.mentor, .prop = mwc::RP_TIMESTAMP, .limit = 1});
      if (lastContrib.empty()) {
        CBL_INFO << "Not posting message on '" << mentorTalkPage << "' because they don't have any contribution.";
      } else if (lastContrib[0].timestamp < Date::now() - DateDiff::fromDays(10)) {
        CBL_INFO << "Not posting message on '" << mentorTalkPage << "' because their last contribution is too old ("
                 << lastContrib[0].timestamp << ").";
      } else if (m_wiki->readRedirect(content, nullptr, nullptr)) {
        CBL_INFO << "Not posting message on '" << mentorTalkPage << "' because it is a redirect.";
      } else if (mwc::testBotExclusion(content, m_wiki->externalUserName(), "notif-réponse-bienvenue") ||
                 content.empty()) {
        CBL_INFO << "Not posting message on '" << mentorTalkPage << "' because it contains a bot exclusion template.";
      } else if (content.find("[[Utilisateur:" + post.user + "|") != string::npos) {
        CBL_ERROR << "Not posting message on '" << mentorTalkPage
                  << "' because it already contains a link to the user page. This is probably a double-posting bug.";
      } else {
        string message = cbl::concat("== ", title, " ==\n", messageBody, "\n\n~~~~");
        CBL_INFO << (dryRun ? "[DRY RUN] " : "") << "Posting notification on '" << mentorTalkPage << "':\n" << message;
        if (!dryRun) {
          cbl::append(content, "\n\n", message);
          summary = cbl::concat("/* ", title, " */ nouvelle section");
        }
        messageSentToMentor = true;
      }
    });
  } catch (const mwc::LowLevelError& error) {
    CBL_ERROR << "Failed to post a message to mentor '" << postAnalysis.mentor << "' (LowLevelError): " << error.what();
    // Depending on when exactly the error occurred, it is possible that the mentor was notified without us getting the
    // confirmation. We return false anyway, which means that in the worst case, the mentee will also get a message.
    return false;
  } catch (const mwc::WikiError& error) {
    CBL_ERROR << "Failed to post a message to mentor '" << postAnalysis.mentor << "': " << error.what();
    return false;
  }
  return messageSentToMentor;
}

void LostMessages::processPosts(const vector<Post>& posts, bool dryRun) {
  vector<const Post*> sortedPosts;
  sortedPosts.reserve(posts.size());
  for (const Post& post : posts) {
    sortedPosts.push_back(&post);
  }
  std::sort(sortedPosts.begin(), sortedPosts.end(), [](const Post* post1, const Post* post2) {
    if (post1->timestamp != post2->timestamp) {
      return post1->timestamp > post2->timestamp;
    } else {
      return post1->revid > post2->revid;
    }
  });

  constexpr int MAX_LINES = 50;
  const DateFormatter& dateFormatter = DateFormatter::getByLang("fr");
  string newPosts;
  int maxRemainingLines = MAX_LINES;
  for (const Post* post : sortedPosts) {
    PostAnalysis postAnalysis;
    if (!analyzePost(*post, postAnalysis)) {
      continue;
    }
    if (postAnalysis.classification.llmLanguage == MessageClassification::Language::OTHER) {
      // The bot can only reply in French or in English for now, so disable it for other languages.
      postAnalysis.classification.llmCategory = MessageClassification::Category::OTHER;
    }

    const MentorState& mentorState = getMentorState(postAnalysis.mentor);
    bool isReplyToMentor = false, isReplyToMentorStrict = false;
    if (!postAnalysis.mentor.empty()) {
      bool mentorNameInDiff =
          RE2::PartialMatch(postAnalysis.diff, cbl::concat("\\b(?i:", RE2::QuoteMeta(postAnalysis.mentor), ")\\b"));
      isReplyToMentor = postAnalysis.sectionType == SectionType::WELCOME_MESSAGE || !post->hasNonWelcomeBotMessage;
      isReplyToMentorStrict = postAnalysis.sectionType == SectionType::WELCOME_MESSAGE ||
                              (!post->hasNonWelcomeBotMessage && mentorNameInDiff);
      // TODO: Either deprecate isReplyToMentorStrict or remove this line.
      isReplyToMentorStrict = isReplyToMentor;
    }

    if (postAnalysis.classification.finalCategory() == MessageClassification::Category::THANKS &&
        postAnalysis.classification.categoryHasHighConfidence() && isReplyToMentor &&
        (!mentorState.thanksForwarded || hasOptedInToReceiveThanks(postAnalysis.mentor))) {
      string title = "Message de " + post->user;
      string message = composeMentorMessage(*post, postAnalysis, mentorState, true);
      if (postOnMentorTalkPage(*post, postAnalysis, title, message, dryRun)) {
        setMentorState(postAnalysis.mentor, true, true);
      }
    } else if (postAnalysis.classification.finalCategory() == MessageClassification::Category::WIKI_QUESTION) {
      if (post->previousRevid == mwc::INVALID_REVID && postAnalysis.diff.find("{{") != string::npos) {
        CBL_INFO << "Not posting message on '" << post->page
                 << "' because the analyzed changed was a page creation that contained a template (probably an "
                    "advanced user)";
        continue;
      }
      bool messageSentToMentor = false;
      if (isReplyToMentorStrict && postAnalysis.classification.categoryHasHighConfidence()) {
        string title = "Question de " + post->user;
        string message = composeMentorMessage(*post, postAnalysis, mentorState, false);
        if (postOnMentorTalkPage(*post, postAnalysis, title, message, dryRun)) {
          setMentorState(postAnalysis.mentor, true, false);
          messageSentToMentor = true;
        }
      }
      if (!messageSentToMentor) {
        string message = composeMessage(*post, postAnalysis);
        try {
          m_wiki->editPage(post->page, [&](string& content, string& summary) {
            if (content != postAnalysis.pageContent) {
              CBL_INFO << "Not posting message on '" << post->page << "' because " << post->revid
                       << " is not the latest revision";
              return;
            }
            CBL_INFO << (dryRun ? "[DRY RUN] " : "") << "Posting answer on '" << post->page << "':\n" << message;
            if (!dryRun) {
              if (postAnalysis.answerStart != -1 && postAnalysis.answerEnd != -1) {
                content = cbl::concat(content.substr(0, postAnalysis.answerStart), "\n", message,
                                      content.substr(postAnalysis.answerEnd));
              } else {
                cbl::append(content, "\n\n", message);
              }
              summary = string(REPLY_COMMENT);
            }
          });
        } catch (const mwc::WikiError& error) {
          CBL_ERROR << "Failed to post a message to '" << post->user << "': " << error.what();
        }
      }
    }

    string formatting =
        postAnalysis.classification.finalCategory() == MessageClassification::Category::WIKI_QUESTION ? "'''" : "";
    cbl::append(
        newPosts, "* ", formatting,
        dateFormatter.format(wikiutil::getFrWikiLocalTime(post->timestamp), DateFormatter::LONG, DateFormatter::MINUTE),
        " : {{u|", post->user, "}} a [[Spécial:Diff/", std::to_string(post->revid), "|modifié]] ",
        postAnalysis.onDraftTalk ? "la page de discussion de son brouillon" : "sa page de discussion",
        " (catégorisation : modèle local = ", getFrenchCategoryName(postAnalysis.classification.localModelCategory),
        " / LLM = ", getFrenchCategoryName(postAnalysis.classification.llmCategory), ")", formatting, "\n");
    maxRemainingLines--;
  }

  if (newPosts.empty()) {
    return;
  }

  m_wiki->editPage("Utilisateur:OrlodrimBot/Messages sans destinataire", [&](string& content, string& summary) {
    vector<string_view> oldLines = cbl::splitLinesAsVector(mwc::readBotSection(content));
    int linesToKeep = std::min(static_cast<int>(oldLines.size()), maxRemainingLines);
    string newBotSection = newPosts;
    for (int i = 0; i < linesToKeep; i++) {
      cbl::append(newBotSection, oldLines[i], "\n");
    }
    if (dryRun) {
      CBL_INFO << "[DRY RUN] Writing bot section:\n" << newBotSection;
    } else {
      mwc::replaceBotSection(content, newBotSection);
    }
    summary = "Mise à jour";
  });
}

void LostMessages::runOnRecentChanges(live_replication::RecentChangesReader& recentChangesReader, json::Value& state,
                                      bool dryRun) {
  vector<Post> posts = extractPostsFromRecentChanges(recentChangesReader, state);
  processPosts(posts, dryRun);
}

void LostMessages::runForUser(const string& user, bool onDraftPage, bool dryRun) {
  string page = cbl::concat("User talk:", user, onDraftPage ? "/Brouillon" : "");
  optional<Post> post = extractPostOfUser({.page = page, .user = user, .revid = mwc::INVALID_REVID});
  if (post.has_value()) {
    processPosts({*post}, dryRun);
  }
}
