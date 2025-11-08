#include "newsletter_distributor.h"
#include <re2/re2.h>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "cbl/date.h"
#include "cbl/error.h"
#include "cbl/file.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/bot_exclusion.h"
#include "mwclient/parser.h"
#include "mwclient/titles_util.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"
#include "orlodrimbot/wikiutil/date_formatter.h"
#include "tweet_proposals.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::LogEvent;
using mwc::NS_TEMPLATE;
using mwc::NS_USER;
using mwc::NS_USER_TALK;
using mwc::RCM_FLOW_BOARD;
using mwc::Revision;
using mwc::RP_CONTENT;
using mwc::RP_CONTENT_MODEL;
using mwc::UG_AUTOPATROLLED;
using mwc::UIP_GROUPS;
using mwc::UserInfo;
using mwc::Wiki;
using std::optional;
using std::string;
using std::string_view;
using std::vector;
using wikicode::getTitleLevel;
using wikiutil::DateFormatter;

namespace newsletters {

constexpr char TWITTER_SUBSCRIBER[] = "<TWITTER>";
constexpr char BISTRO_SUBSCRIBER[] = "Wikipédia:Le Bistro";

static void splitCodeBySections(const string& code, vector<string>& sections) {
  sections.clear();
  sections.emplace_back();
  for (string_view line : cbl::splitLines(code)) {
    if (getTitleLevel(line) != 0) {
      sections.emplace_back();
    }
    cbl::append(sections.back(), line, "\n");
  }
}

vector<Subscriber> getSubscribers(Wiki& wiki, const string& subscriptionPage) {
  static const re2::RE2 reTarget(R"(\s*(?i:#target:)(.*))");
  string code = wiki.readPageContent(subscriptionPage);
  vector<Subscriber> subscribers;
  for (string_view line : cbl::splitLines(code)) {
    if (!line.starts_with("*") && !line.starts_with("#")) continue;
    wikicode::List parsedLine = wikicode::parse(line);
    string targetPage;
    Subscriber subscriber;
    for (const wikicode::Template& template_ : parsedLine.getTemplates()) {
      string templateName = wiki.normalizeTitle(template_.name(), NS_TEMPLATE);
      if (templateName == "Modèle:BeBot nopurge" || templateName == "Modèle:Ne pas purger les anciens numéros") {
        subscriber.deleteOldMessages = false;
      } else if (templateName == "Modèle:Abonnement Bistro") {
        subscriber.page = BISTRO_SUBSCRIBER;
        subscriber.deleteOldMessages = false;
      } else if (subscriber.page.empty() && RE2::FullMatch(template_[0].toString(), reTarget, &targetPage)) {
        mwc::TitleParts parsedTitle = wiki.parseTitle(targetPage);
        if (parsedTitle.namespaceNumber == NS_USER) {
          // Corner case: if the subscriber is "Utilisateur:", subscriber.page will be empty. That's OK.
          subscriber.page = wiki.getTalkPage(parsedTitle.title);
        } else if (parsedTitle.namespaceNumber == NS_USER_TALK ||
                   parsedTitle.namespaceNumber == 101 /* Discussion Portail */ ||
                   parsedTitle.namespaceNumber == 103 /* Discussion Projet */) {
          subscriber.page = parsedTitle.title;
        }
      }
    }
    if (!subscriber.page.empty()) {
      subscribers.push_back(subscriber);
    }
  }
  return subscribers;
}

optional<string> getUserFromPage(Wiki& wiki, const string& title) {
  mwc::TitleParts titleParts = wiki.parseTitle(title);
  if (titleParts.namespaceNumber != mwc::NS_USER && titleParts.namespaceNumber != mwc::NS_USER_TALK) {
    return std::nullopt;
  }
  string user(titleParts.unprefixedTitle());
  size_t slashPosition = user.find('/');
  if (slashPosition != string::npos) {
    user.resize(slashPosition);
  }
  return user;
}

optional<Revision> getLastContribution(Wiki& wiki, const string& user) {
  vector<mwc::Revision> contribs;
  mwc::UserContribsParams contribsParams;
  contribsParams.user = user;
  contribsParams.limit = 1;
  contribsParams.prop = mwc::RP_TIMESTAMP;
  contribs = wiki.getUserContribs(contribsParams);
  if (contribs.empty()) {
    return std::nullopt;
  }
  return contribs[0];
}

Distributor::Result::Result(const string& issueTitle, const string& internalError, const string& displayableError)
    : ok(false), issueTitle(issueTitle), displayableError(displayableError) {
  if (issueTitle.empty()) {
    this->internalError = "Failed to publish '" + issueTitle + "': " + internalError;
  } else {
    this->internalError = internalError;
  }
}

Distributor::Distributor(Wiki* wiki, const string& stateFile,
                         live_replication::RecentChangesReader* recentChangesReader)
    : m_wiki(wiki), m_stateFile(stateFile), m_recentChangesReader(recentChangesReader) {
  loadState();
}

Distributor::~Distributor() {}

bool Distributor::run(const string& forcedIssue, const string& fromPage, const string& singlePage, bool force,
                      bool dryRun) {
  try {
    runInternal(forcedIssue, fromPage, singlePage, force, dryRun);
  } catch (const mwc::WikiError& error) {
    CBL_ERROR << error.what();
    return false;
  } catch (const DistributorError& error) {
    CBL_ERROR << error.what();
    if (!error.displayableError().empty()) {
      if (dryRun) {
        CBL_INFO << "[DRY RUN] Failure notification for '" << error.issueTitle() << "': \"" << error.displayableError()
                 << "\"";
      } else {
        sendFailureNotification(error.issueTitle(), error.displayableError());
      }
    }
    return false;
  }
  return true;
}

bool Distributor::wasDistributed(const std::string& issue) {
  return !compareIssues(m_state["lastissue"].str(), issue);
}

void Distributor::loadState() {
  m_state.setNull();
  try {
    m_state = json::parse(cbl::readFile(m_stateFile));
  } catch (const cbl::FileNotFoundError& error) {
    CBL_WARNING << "State file '" << m_stateFile << "' does not exist";
  } catch (const cbl::ParseError& error) {
    CBL_ERROR << "Cannot parse state file '" << m_stateFile << "': " << error.what();
  }
}

void Distributor::saveState(bool dryRun) {
  string stateJSON = m_state.toJSON();
  if (dryRun) {
    CBL_INFO << "[DRY RUN] Saving state " << stateJSON;
  } else {
    stateJSON += '\n';
    cbl::writeFile(m_stateFile, stateJSON);
    ;
  }
}

Distributor::Result Distributor::isUserAllowedToPublish(const string& user) {
  vector<UserInfo> userInfo(1);
  userInfo[0].name = user;
  m_wiki->getUsersInfo(UIP_GROUPS, userInfo);
  if (userInfo[0].groups & UG_AUTOPATROLLED) {
    return Result();
  } else {
    return Result("", user + " is not autopatrolled", "{{u'|" + user + "}} n'est pas autopatrolled.");
  }
}

string Distributor::getNewIssue(bool dryRun) {
  CBL_ASSERT(m_recentChangesReader != nullptr);
  string continueToken = m_state["rcContinueToken"].str();
  live_replication::RecentLogEventsOptions options;
  options.logType = mwc::LE_MOVE;
  if (continueToken.empty()) {
    options.start = Date::now() - DateDiff::fromHours(1);
  }
  options.continueToken = &continueToken;
  Date enumStart;
  vector<LogEvent> moves = m_recentChangesReader->getRecentLogEvents(options);

  string newIssue;
  Result allowPublicationResult;
  string expectedPrefix = getSubpagesPrefix();
  for (vector<LogEvent>::reverse_iterator moveIt = moves.rbegin(); moveIt != moves.rend(); ++moveIt) {
    const string& issue = moveIt->moveParams().newTitle;
    if (issue.starts_with(expectedPrefix)) {
      Result currentIssueResult = canBeCurrentIssueTitle(issue);
      if (currentIssueResult.ok) {
        // If this raises a WikiError, the state should not be saved.
        allowPublicationResult = isUserAllowedToPublish(moveIt->user);
        newIssue = issue;
        break;
      }
      CBL_WARNING << "Skipping '" << issue << "': " << currentIssueResult.internalError;
    }
  }

  m_state.getMutable("rcContinueToken") = continueToken;
  saveState(dryRun);
  if (!newIssue.empty() && !allowPublicationResult.ok) {
    throw DistributorError(allowPublicationResult.issueTitle, allowPublicationResult.internalError,
                           allowPublicationResult.displayableError);
  }
  return newIssue;
}

bool Distributor::isValidTargetPage(const string& targetPage, const string& originalPage) {
  const int originalNamespace_ = m_wiki->getTitleNamespace(originalPage);
  const int namespace_ = m_wiki->getTitleNamespace(targetPage);
  if (originalNamespace_ == NS_USER || originalNamespace_ == NS_USER_TALK) {
    return namespace_ == NS_USER_TALK || targetPage.find('/') != string::npos;
  } else if (originalNamespace_ == 101 || originalNamespace_ == 103) {
    return namespace_ == 101 || namespace_ == 103;
  }
  return false;
}

void Distributor::postMessage(const string& issue, const string& targetPage, bool deleteOld, bool dryRun) {
  string messageTitle;
  string messageNowikiTitle;
  string messageContent;
  string editSummary;
  prepareMessage(issue, messageTitle, messageNowikiTitle, messageContent, editSummary);
  if (messageNowikiTitle.empty()) {
    messageNowikiTitle = messageTitle;
  }
  if (editSummary.empty()) {
    editSummary = "/* " + messageNowikiTitle + " */ nouvelle section";
  }

  CBL_INFO << "Posting message on '" << targetPage << "'";
  string resolvedTargetPage = targetPage;
  Revision revision;
  mwc::WriteToken writeToken;
  string redirectInfo;
  for (int numRedirects = 0;; numRedirects++) {
    if (resolvedTargetPage == BISTRO_SUBSCRIBER) {
      resolvedTargetPage =
          "Wikipédia:Le Bistro/" + DateFormatter::getByLang("fr").format(Date::now() + DateDiff::fromHours(6));
    } else if (!isValidTargetPage(resolvedTargetPage, targetPage)) {
      throw UpdatePageError("Page '" + resolvedTargetPage + "' is not a valid target");
    }
    try {
      revision = m_wiki->readPage(resolvedTargetPage, RP_CONTENT | RP_CONTENT_MODEL, &writeToken);
    } catch (const mwc::PageNotFoundError&) {
      if (numRedirects != 0) {
        throw;
      }
      optional<string> user = getUserFromPage(*m_wiki, resolvedTargetPage);
      if (!user.has_value() || !getLastContribution(*m_wiki, *user).has_value()) {
        throw;
      }
      revision = Revision();
      revision.contentModel = mwc::RCM_WIKITEXT;
      writeToken = mwc::WriteToken::newForCreation();
    }
    string redirTarget;
    if (!m_wiki->readRedirect(revision.content, &redirTarget, nullptr)) {
      break;
    } else if (numRedirects != 0) {
      throw UpdatePageError("Page '" + targetPage + "' is a double or recursive redirect");
    }
    CBL_INFO << "Following redirect from '" << resolvedTargetPage << "' to '" << redirTarget << "' for the newsletter";
    {
      mwc::TitleParts titleParts = m_wiki->parseTitle(resolvedTargetPage);
      string originalTargetLink = m_wiki->makeLink(resolvedTargetPage);
      string subscriber = titleParts.namespaceNumber == NS_USER || titleParts.namespaceNumber == NS_USER_TALK
                              ? cbl::concat("{{u'|", titleParts.unprefixedTitle(), "}}")
                              : originalTargetLink;
      redirectInfo =
          cbl::concat("\n\n<small>Ce message vous est adressé car ", subscriber, " est ", getSubscribedToString(),
                      " et ", originalTargetLink,
                      " redirige ici. Si vous avez renommé votre compte, pensez à mettre à jour votre nom dans la [[",
                      getSubscriptionPage(),
                      "|liste des abonnés]] pour ne plus voir cet avertissement. À l'inverse, si cette redirection est "
                      "une erreur, [[Special:EditPage/",
                      resolvedTargetPage, "|supprimez-la]] pour que les messages ne soient plus transmis.</small>");
    }
    resolvedTargetPage = redirTarget;
  }

  if (revision.contentModel == RCM_FLOW_BOARD) {
    if (!dryRun) {
      m_wiki->flowNewTopic(resolvedTargetPage, messageNowikiTitle, messageContent + redirectInfo);
    } else {
      CBL_INFO << "[DRY RUN] Create flow topic on '" << resolvedTargetPage << "'";
    }
  } else {
    if (mwc::testBotExclusion(revision.content, m_wiki->externalUserName(), "")) {
      throw UpdatePageError("Edition is prevented by a bot exclusion template");
    }
    vector<string> sections;
    splitCodeBySections(revision.content, sections);
    string deletedCode;
    string* previousNewsletterSection = nullptr;

    for (string& section : sections) {
      const string sectionIssue = getIssueFromSection(section);
      if (sectionIssue == issue) {
        CBL_INFO << "The current issue is already on the page";
        return;
      } else if (!sectionIssue.empty() && deleteOld) {
        if (previousNewsletterSection != nullptr) {
          if (!isStandardNewsletterSection(*previousNewsletterSection)) {
            CBL_WARNING << "Keeping section of a previous issue because a change was detected in the section";
            CBL_INFO << *previousNewsletterSection;
          } else {
            deletedCode += *previousNewsletterSection;
            previousNewsletterSection->clear();
          }
        }
        previousNewsletterSection = &section;
      }
    }

    string newCode;
    for (const string& section : sections) {
      newCode += section;
    }
    const string message = cbl::concat("== ", messageTitle, " ==\n", messageContent, " ~~~~", redirectInfo);
    newCode += '\n';
    newCode += message;

    std::cout << "<<<<<<<<\n" << deletedCode;
    std::cout << ">>>>>>>>\n" << message << "\n";
    std::cout << "comment=" << editSummary << "\n";
    if (!dryRun) {
      m_wiki->writePage(resolvedTargetPage, newCode, writeToken, editSummary);
    } else {
      CBL_INFO << "[DRY RUN] Writing '" << resolvedTargetPage << "'";
    }
  }
}

void Distributor::addTweetProposal(const string& issue, int issueNumber, bool dryRun) {
  if (issueNumber < 100 || issueNumber >= 100000) {
    throw UpdatePageError("Invalid issue number: " + std::to_string(issueNumber));
  }

  TweetProposals tweetProposals(m_wiki);
  tweetProposals.load();

  string text, image, editSummary;
  prepareTweet(issue, issueNumber, text, image, editSummary);
  if (text.empty()) {
    throw UpdatePageError("prepareTweet did not return any text");
  }

  // clang-format off
  string tweetProposal =
      "{{Proposition tweet\n"
      "|texte=" + text + "\n"
      "|média=" + image + "\n"
      "|mode=bot\n"
      "|proposé par=~~~~\n"
      "|validé par=\n"
      "|publié par=\n"
      "}}\n\n";
  // clang-format on
  try {
    tweetProposals.addProposal(tweetProposal);
  } catch (const cbl::ParseError& error) {
    throw UpdatePageError(error.what());
  }

  if (dryRun) {
    CBL_INFO << "[DRY RUN] Proposition de tweet:\n" << tweetProposal;
  } else {
    tweetProposals.writePage(editSummary);
  }
}

void Distributor::runInternal(const string& forcedIssue, const string& fromPage, const string& singlePage, bool force,
                              bool dryRun) {
  string newIssue = forcedIssue;
  if (newIssue.empty()) {
    newIssue = getNewIssue(dryRun);
    if (newIssue.empty()) return;
    CBL_INFO << "New issue: " << newIssue;
  }

  string previousIssue = m_state["lastissue"].str();
  if (!compareIssues(previousIssue, newIssue)) {
    string message = "The last published issue is " + previousIssue + " (>= " + newIssue + ")";
    if (!force) {
      throw DistributorError(newIssue, message,
                             newIssue == previousIssue
                                 ? "ce numéro a déjà été distribué."
                                 : "ce numéro est antérieur au dernier numéro distribué (" + previousIssue + ").");
    }
    CBL_WARNING << "Forcing publication despite the following error: " << message;
  }

  int issueNumber = 0;
  string displayableError;
  Result result = isIssueReadyForPublication(newIssue, issueNumber);
  if (!result.ok) {
    if (!force) {
      throw DistributorError(result.issueTitle, result.internalError, result.displayableError);
    }
    CBL_WARNING << "Forcing publication despite the following error: " << result.internalError;
  }
  vector<Subscriber> subscribers = getSubscribers(*m_wiki, getSubscriptionPage());
  if (subscribers.empty()) {
    throw DistributorError(newIssue, "no subscriber found",
                           "aucun inscrit trouvé sur [[" + getSubscriptionPage() + "]]");
  }

  if (enableTwitterPublication()) {
    Subscriber tweeterSubscriber;
    tweeterSubscriber.page = TWITTER_SUBSCRIBER;
    subscribers.insert(subscribers.begin(), tweeterSubscriber);
  }

  if (compareIssues(m_state["lastissue"].str(), newIssue)) {
    m_state.getMutable("lastissue") = newIssue;
    saveState(dryRun);
  }

  bool afterStartPoint = fromPage.empty();
  for (const Subscriber& subscriber : subscribers) {
    if (!singlePage.empty() && subscriber.page != singlePage) {
      continue;
    }
    if (!afterStartPoint) {
      if (subscriber.page == fromPage) {
        afterStartPoint = true;
      } else {
        continue;
      }
    }
    try {
      if (subscriber.page == TWITTER_SUBSCRIBER) {
        addTweetProposal(newIssue, issueNumber, dryRun);
      } else {
        postMessage(newIssue, subscriber.page, subscriber.deleteOldMessages, dryRun);
      }
    } catch (const mwc::WikiError& error) {
      CBL_ERROR << error.what();
    } catch (const UpdatePageError& error) {
      CBL_ERROR << error.what();
    }
  }
}

}  // namespace newsletters
