#include "article_to_draft_move.h"
#include <re2/re2.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/bot_exclusion.h"
#include "mwclient/parser.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/wiki_job_runner/job_queue/job_queue.h"
#include "orlodrimbot/wikiutil/date_parser.h"
#include "orlodrimbot/wikiutil/escape_comment.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::LogEvent;
using mwc::Revision;
using mwc::Wiki;
using std::make_unique;
using std::pair;
using std::string;
using std::string_view;
using std::vector;

constexpr const char ARTICLE_TO_DRAFT_MOVE_HANDLER_ID[] = "article_to_draft_move";

pair<string_view, string_view> extractUserAndSubpage(string_view title) {
  string_view user;
  string_view subpage;
  size_t colonPosition = title.find(':');
  if (colonPosition != string::npos) {
    size_t slashPosition = title.find('/', colonPosition + 1);
    if (slashPosition != string::npos) {
      user = title.substr(colonPosition + 1, slashPosition - colonPosition - 1);
      if (user.ends_with(" ")) {
        user.remove_suffix(1);
      }
      subpage = title.substr(slashPosition);
    }
  }
  return {user, subpage};
}

bool containsLink(Wiki& wiki, const string& content, const string& link) {
  wikicode::List parsedCode = wikicode::parse(content);
  string normalizedLink = wiki.normalizeTitle(link);
  for (wikicode::Link& link : parsedCode.getLinks()) {
    if (wiki.normalizeTitle(link.target()) == normalizedLink) {
      return true;
    }
  }
  return false;
}

void ArticleToDraftMoveHandler::run(const job_queue::Job& job, job_queue::JobQueue& jobQueue, bool dryRun) {
  CBL_INFO << "ArticleToDraftMoveHandler: processing '" << job.key << "'";
  vector<Revision> history;
  try {
    history = m_wiki->getHistory({.title = job.key, .prop = mwc::RP_USER, .direction = mwc::OLDEST_FIRST, .limit = 1});
  } catch (const mwc::PageNotFoundError&) {
    CBL_INFO << "Page '" << job.key << "' does not exist";
    return;
  }
  if (history.empty()) {
    CBL_INFO << "Page '" << job.key << "' has no history";
    return;
  }
  string_view user = extractUserAndSubpage(job.key).first;
  if (history[0].user != user) {
    CBL_INFO << "Page '" << job.key << "' was created by '" << history[0].user << "', not '" << user << "'";
    return;
  }
  vector<mwc::UserInfo> users(1);
  users[0].name = string(user);
  m_wiki->getUsersInfo(mwc::UIP_EDIT_COUNT, users);
  if (users[0].editCount > 200) {
    CBL_INFO << "User '" << user << "' already has many edits (" << users[0].editCount << ")";
    return;
  }
  string mover = job.parameters["mover"].str();
  if (user == mover) {
    CBL_INFO << "Page '" << job.key << "' was moved its creator";
    return;
  }

  string article = job.parameters["article"].str();
  string standardDraft = cbl::concat("Utilisateur:", user, "/Brouillon");
  Date moveTimestamp = Date::fromISO8601OrEmpty(job.parameters["move_timestamp"].str());
  string userTalkPage = cbl::concat("Discussion utilisateur:", user);
  string formattedComment = wikiutil::escapeComment(*m_wiki, job.parameters["comment"].str());
  string commonParameters =
      cbl::concat("|article=", article, "|utilisateur=", job.parameters["mover"].str(), "|brouillon=", job.key);
  bool botExclusion = false;

  m_wiki->editPage(userTalkPage, [&](string& content, string& summary) {
    static const re2::RE2 reSpecialIndex(R"((?i:\[\[Sp[ée]cial:Index/(Utilisateur|Utilisatrice|User):))");
    if (mwc::testBotExclusion(content, m_wiki->externalUserName(), "article-vers-brouillon")) {
      CBL_INFO << "The page '" << userTalkPage << "' contains a bots exclusion template that blocks the message";
      botExclusion = true;
      return;
    } else if (containsLink(*m_wiki, content, job.key)) {
      CBL_INFO << "The page '" << userTalkPage << "' already contains a link to '" << job.key
               << "' so no message will be sent to the user";
      return;
    } else if (RE2::PartialMatch(content, reSpecialIndex)) {
      Date latestMessage = wikiutil::DateParser::getByLang("fr").extractMaxSignatureDate(content).utcDate;
      if (!moveTimestamp.isNull() && latestMessage >= moveTimestamp - DateDiff::fromMinutes(1)) {
        CBL_INFO << "The page '" << userTalkPage
                 << "' already contains a link to user subpages and was modified after the draft move";
        return;
      }
      CBL_INFO << "The page '" << userTalkPage
               << "' already contains a link to user subpages but was not modified recently (moveTimestamp="
               << moveTimestamp << ", latestMessage=" << latestMessage << ")";
    } else if (m_wiki->readRedirect(content, nullptr, nullptr)) {
      CBL_INFO << "The page '" << userTalkPage << "' is a redirect so no message will be sent to the user";
      return;
    }
    cbl::append(content, content.empty() ? "" : "\n\n",
                "{{subst:Utilisateur:OrlodrimBot/Message article transformé en brouillon", commonParameters,
                "|commentaire=", formattedComment, "}}");
    summary =
        cbl::concat("Notification de la transformation de l'article « ", article, " » en [[", job.key, "|brouillon]]");
    CBL_INFO << "ArticleToDraftMoveHandler: posting message on '" << userTalkPage << "'";
  });
  if (botExclusion) {
    return;
  }

  if (job.key == standardDraft) {
    // The draft is already the page that can be found by clicking on the "Brouillon" link in the UI.
    return;
  }
  if (mwc::getAccountType(user) != mwc::AccountType::USER) {
    // Temporary accounts do not have a "Brouillon" link.
    return;
  }
  m_wiki->editPage(standardDraft, [&](string& content, string& summary) {
    if (mwc::testBotExclusion(content, m_wiki->externalUserName(), "article-vers-brouillon")) {
      CBL_INFO << "The page '" << standardDraft << "' contains bots exclusion template that blocks the message";
      return;
    }
    string redirectTarget;
    if (m_wiki->readRedirect(content, &redirectTarget, nullptr)) {
      if (content.find('\n') != string::npos) {
        CBL_INFO << "The page '" << standardDraft
                 << "' is a redirect but has content at the same time, so it cannot be erased";
        return;
      } else if (m_wiki->getTitleNamespace(redirectTarget) != mwc::NS_MAIN) {
        CBL_INFO << "The page '" << standardDraft << "' is a redirect to '" << redirectTarget
                 << "' which is not in the main namespace, so it cannot be erased";
        return;
      }
      content.clear();
    }
    int numInclusions = 0;
    for (string_view line : cbl::splitLines(content)) {
      if (line.find("{{Lien vers article transformé en brouillon|") != string::npos) {
        if (line.find(cbl::concat("|brouillon=", job.key, "}}")) != string::npos) {
          CBL_INFO << "The template is already included for the same draft, not adding it again";
          return;
        }
        numInclusions++;
      }
    }
    if (numInclusions >= 5) {
      CBL_INFO << "The template is already included 5 times or more, not adding it again";
      return;
    }
    if (dryRun) {
      CBL_INFO << "[DRY RUN] Updating '" << job.key << "'";
      return;
    }
    content = cbl::concat("{{Lien vers article transformé en brouillon", commonParameters, "}}\n", content);
    summary = cbl::concat("Ajout d'un lien vers [[", job.key, "]]");
    CBL_INFO << "ArticleToDraftMoveHandler: adding banner on '" << standardDraft << "'";
  });
}

bool isArticleToDraftMove(Wiki& wiki, const LogEvent& logEvent) {
  if (logEvent.type() != mwc::LE_MOVE) {
    return false;
  }
  const string& article = logEvent.title;
  string_view draft = logEvent.moveParams().newTitle;
  if (wiki.getTitleNamespace(article) != mwc::NS_MAIN || wiki.getTitleNamespace(draft) != mwc::NS_USER) {
    return false;
  }
  auto [user, subpage] = extractUserAndSubpage(draft);
  return !user.empty() && user != logEvent.user && mwc::getAccountType(user) != mwc::AccountType::IP &&
         !subpage.empty();
}
