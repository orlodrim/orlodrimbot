#include "move_subpages_lib.h"
#include <re2/re2.h>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/wiki.h"

using cbl::Date;
using mwc::LE_MOVE;
using mwc::LogEvent;
using mwc::RP_TIMESTAMP;
using mwc::RP_TITLE;
using std::string;
using std::unordered_map;
using std::unordered_set;
using std::vector;

constexpr int MAX_EVENTS = 2000;
constexpr int MAX_SUBPAGES = 50;

const re2::RE2 SUBPAGE_REGEXP(
    "/(Admissibilité|Suppression|Article de qualité|Bon article|Droit d'auteur|Neutralité|Traduction|[Aa]rchive.*|"
    "À faire)");

const unordered_set<int> NAMESPACES_TO_PROCESS = {
    mwc::NS_MAIN,
    mwc::NS_PROJECT,
    mwc::NS_TEMPLATE,
    mwc::NS_HELP,
};

void SubpagesMover::processAllMoves() {
  CBL_INFO << "Enumerating page moves since " << m_dateOfLastProcessedMove;
  vector<LogEvent> logEvents = readMoveLog(m_dateOfLastProcessedMove);
  if (logEvents.empty()) {
    return;
  }
  Date mostRecentLogEvent = logEvents[0].timestamp;

  // Deal with multiple consecutive moves, e.g. A → B followed by B → C.
  // We keep all steps, but replace all intermediate targets by the final target (e.g. A → B becomes A → C and B → C
  // remains unchanged).
  // - Typical case: subpages of Talk:A were never moved. Processing A → C will move them in a single step. Processing
  //   B → C will do nothing.
  // - Special case: subpages of Talk:A were only moved during the first move, but not during the second. Since we also
  //   kept B → C, they will be moved correctly as well.
  unordered_map<string, string> lastName;
  for (LogEvent& logEvent : logEvents) {
    if (lastName.count(logEvent.moveParams().newTitle) != 0) {
      logEvent.mutableMoveParams().newTitle = lastName[logEvent.moveParams().newTitle];
    }
    lastName[logEvent.title] = logEvent.moveParams().newTitle;
  }

  // Only keep moves in the same namespace, and only if the namespace is in NAMESPACES_TO_PROCESS.
  // This is done after collapsing consecutive moves because moving pages to the wrong namespace while renaming them and
  // doing a second move to fix the namespace is a common mistake.
  logEvents.erase(std::remove_if(logEvents.begin(), logEvents.end(),
                                 [&](const LogEvent& logEvent) {
                                   int oldNamespace = m_wiki->getTitleNamespace(logEvent.title);
                                   int newNamespace = m_wiki->getTitleNamespace(logEvent.moveParams().newTitle);
                                   return NAMESPACES_TO_PROCESS.count(oldNamespace) == 0 ||
                                          newNamespace != oldNamespace ||
                                          logEvent.title == logEvent.moveParams().newTitle;
                                 }),
                  logEvents.end());

  // Process the renaming moves.
  int logEventIndex = 0;
  for (const LogEvent& logEvent : logEvents) {
    logEventIndex++;
    CBL_INFO << "Reading subpages of '" << logEvent.title << "' (" << logEventIndex << " / " << logEvents.size()
             << ", date=" << logEvent.timestamp << ")";
    try {
      processMove(logEvent);
    } catch (const mwc::WikiError& error) {
      CBL_ERROR << error.what();
    }
  }

  m_dateOfLastProcessedMove = mostRecentLogEvent;
}

void SubpagesMover::processMove(const LogEvent& logEvent) {
  const string& oldPageTitle = logEvent.title;
  const string& newPageTitle = logEvent.moveParams().newTitle;
  string talkPage = m_wiki->getTalkPage(oldPageTitle);
  string newTalkPage = m_wiki->getTalkPage(newPageTitle);
  if (talkPage.empty() || newTalkPage.empty()) {
    CBL_INFO << "Failed to get the talk page of '" << oldPageTitle << "' or '" << newPageTitle << "'";
    return;
  }

  vector<string> subpages = getSubpages(talkPage);
  if (subpages.empty()) return;

  string oldPageCode;
  try {
    oldPageCode = m_wiki->readPageContent(oldPageTitle);
  } catch (const mwc::PageNotFoundError&) {
    oldPageCode.clear();
  }

  if (newPageTitle.starts_with(oldPageTitle + "/")) {
    CBL_INFO << "Ignoring subpages of '" << oldPageTitle << "' because the new page '" << newPageTitle
             << "' is a subpage of the old one";
    return;
  } else if (!oldPageCode.empty() && !m_wiki->readRedirect(oldPageCode, nullptr, nullptr) &&
             !m_wiki->getPagesDisambigStatus({oldPageTitle})[oldPageTitle]) {
    CBL_INFO << "Ignoring subpages of '" << oldPageTitle
             << "' because the old page still exists and is neither a redirect nor a disambiguation page";
    return;
  } else if (!m_wiki->pageExists(newPageTitle)) {
    CBL_INFO << "Ignoring subpages of '" << oldPageTitle << "' because the new page '" << newPageTitle
             << "' no longer exists";
    return;
  } else if (oldPageTitle == "Wikipédia:RAW/Rédaction") {
    // Special page moved each month to publish the new issue of RAW.
    return;
  }

  string comment = "Renommage des sous-pages de discussion, suite au renommage de la page [[" + oldPageTitle + "]]";
  bool anyMoveDone = false;

  for (const string& oldSubpage : subpages) {
    if (!oldSubpage.starts_with(talkPage + "/")) {
      CBL_ERROR << "Internal error: '" << oldSubpage << "' is not a subpage of '" << talkPage << "'";
      continue;
    }
    const string subpageSuffix = oldSubpage.substr(talkPage.size());
    if (!RE2::FullMatch(subpageSuffix, SUBPAGE_REGEXP)) {
      CBL_WARNING << "Ignoring subpage '" << oldSubpage << "' because it does not have a standard subpage name";
    } else {
      string oldSubpageCode = m_wiki->readPageContent(oldSubpage);
      if (oldSubpageCode.empty()) {
        CBL_INFO << "Ignoring subpage '" << oldSubpage << "' because it is empty";
      } else if (m_wiki->readRedirect(oldSubpageCode, nullptr, nullptr)) {
        CBL_INFO << "Ignoring subpage '" << oldSubpage << "' because it is a redirect";
      } else {
        string newSubpage = newTalkPage + subpageSuffix;
        bool createRedirect = true;
        if (subpageSuffix == "/À faire") {
          // Never create a redirect for todo pages because:
          // - Their content is not permanent, so any link to them eventually becomes obsolete anyway.
          // - They are displayed by transclusion in the talk page, so leaving redirects is confusing.
          createRedirect = false;
        } else if (subpageSuffix == "/Archive Commons") {
          // Archive of bot-generated content that should rarely be useful. Avoid creating a redirect if possible.
          vector<string> backlinks = m_wiki->getBacklinks(mwc::BacklinksParams{.title = oldSubpage, .limit = 2});
          createRedirect = !backlinks.empty();
        }
        if (m_dryRun) {
          CBL_INFO << "[DRY RUN] Moving '" << oldSubpage << "' to '" << newSubpage
                   << "' (createRedirect=" << createRedirect << ")";
        } else {
          try {
            m_wiki->movePage(oldSubpage, newSubpage, comment, createRedirect ? 0 : mwc::MOVE_NOREDIRECT);
            anyMoveDone = true;
          } catch (const mwc::PageAlreadyExistsError&) {
            CBL_INFO << "Could not rename '" << oldSubpage << "' because '" << newSubpage << "' already exists";
          } catch (const mwc::WikiError& error) {
            CBL_ERROR << error.what();
          }
        }
      }
    }
  }

  if (anyMoveDone) {
    try {
      // Dummy edit to update categories (https://fr.wikipedia.org/wiki/Special:Diff/194898075).
      // The page may be empty, but in that case it won't have any categories, so we skip that case instead of passing
      // EDIT_ALLOW_BLANKING.
      mwc::WriteToken writeToken;
      string newTalkPageContent = m_wiki->readPageContent(newTalkPage, &writeToken);
      if (!newTalkPageContent.empty()) {
        CBL_INFO << "Performing a dummy edit on '" << newTalkPage << "'";
        m_wiki->writePage(newTalkPage, newTalkPageContent, writeToken);
      }
    } catch (const mwc::PageNotFoundError&) {
      // Nothing to do.
    } catch (const mwc::WikiError& error) {
      CBL_ERROR << error.what();
    }
  }
}

vector<LogEvent> SubpagesMover::readMoveLog(const Date& dateMin) {
  mwc::LogEventsParams params;
  params.prop = RP_TITLE | RP_TIMESTAMP;
  params.type = LE_MOVE;
  params.end = dateMin;
  params.limit = MAX_EVENTS;
  return m_wiki->getLogEvents(params);
}

vector<string> SubpagesMover::getSubpages(const string& title) {
  mwc::AllPagesParams params;
  mwc::TitleParts titleParts = m_wiki->parseTitle(title);
  string unprefixedTitle;
  params.namespace_ = titleParts.namespaceNumber;
  params.prefix = cbl::concat(titleParts.unprefixedTitle(), "/");
  params.limit = MAX_SUBPAGES;
  return m_wiki->getAllPages(params);
}
