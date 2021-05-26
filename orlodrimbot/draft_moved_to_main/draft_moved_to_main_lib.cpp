#include "draft_moved_to_main_lib.h"
#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "cbl/date.h"
#include "cbl/error.h"
#include "cbl/file.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/parser.h"
#include "mwclient/util/bot_section.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"
#include "orlodrimbot/wikiutil/date_formatter.h"
#include "orlodrimbot/wikiutil/date_parser.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::LogEvent;
using mwc::NS_MAIN;
using mwc::UserInfo;
using mwc::Wiki;
using std::deque;
using std::map;
using std::string;
using std::string_view;
using std::unordered_map;
using std::unordered_set;
using std::vector;
using wikiutil::DateFormatter;
using wikiutil::DateParser;

namespace {

class EventsByDay {
public:
  void addEventsFromCode(string_view code);
  void addEvent(const Date& date, const string& event);
  void removeOldEvents(int daysToKeep);

  string toString() const;

private:
  using Section = deque<string>;
  using SectionMap = map<Date, Section, std::greater<Date>>;
  SectionMap m_sections;
};

void EventsByDay::addEventsFromCode(string_view code) {
  const DateParser& dateParser = DateParser::getByLang("fr");
  Section* currentSection = nullptr;
  for (string_view line : cbl::splitLines(code)) {
    if (wikicode::getTitleLevel(line) != 0) {
      Date dateOfSection = dateParser.parseDate(wikicode::getTitleContent(line), DateParser::AFTER_2000);
      currentSection = !dateOfSection.isNull() ? &m_sections[dateOfSection] : nullptr;
    } else if (currentSection && cbl::startsWith(line, "*")) {
      currentSection->emplace_back(line);
    }
  }
}

string EventsByDay::toString() const {
  string code;
  for (const auto& [date, lines] : m_sections) {
    if (!code.empty()) code += '\n';
    code += "== " + DateFormatter::getByLang("fr").format(date, DateFormatter::LONG_1ST_TEMPLATE) + " ==\n";
    for (const string& line : lines) {
      code += line;
      code += '\n';
    }
  }
  return code;
}

void EventsByDay::addEvent(const Date& date, const string& event) {
  CBL_ASSERT(cbl::startsWith(event, "*"));
  m_sections[date.extractDay()].push_front(event);
}

void EventsByDay::removeOldEvents(int daysToKeep) {
  Date minDate = (Date::now() - DateDiff(86400 * daysToKeep)).extractDay();
  for (SectionMap::iterator it = m_sections.end(); it != m_sections.begin();) {
    --it;
    if (it->first >= minDate) break;
    it = m_sections.erase(it);
  }
}

unordered_set<string> getTrustedUsers(Wiki& wiki, const unordered_set<string>& users) {
  vector<UserInfo> userInfoVec;
  for (const string& user : users) {
    userInfoVec.emplace_back();
    userInfoVec.back().name = user;
  }
  std::sort(userInfoVec.begin(), userInfoVec.end(),
            [](const UserInfo& u1, const UserInfo& u2) { return u1.name < u2.name; });
  wiki.getUsersInfo(mwc::UIP_GROUPS, userInfoVec);
  unordered_set<string> trustedUsers;
  for (const UserInfo userInfo : userInfoVec) {
    if (userInfo.groups & (mwc::UIG_AUTOPATROLLED | mwc::UIG_SYSOP | mwc::UIG_BOT)) {
      trustedUsers.insert(userInfo.name);
    }
  }
  return trustedUsers;
}

json::Value loadState(const string& stateFile) {
  try {
    return json::parse(cbl::readFile(stateFile));
  } catch (const cbl::FileNotFoundError&) {
    // Ignored.
  } catch (const cbl::SystemError& error) {
    CBL_ERROR << "Cannot load state: " << error.what();
  } catch (const cbl::ParseError& error) {
    CBL_ERROR << "Cannot parse state from '" << stateFile << "': " << error.what();
  };
  return json::Value();
}

void saveState(const string& stateFile, const json::Value& state) {
  try {
    cbl::writeFile(stateFile, state.toJSON() + "\n");
  } catch (const cbl::SystemError& error) {
    CBL_ERROR << "Cannot save state: " << error.what();
  }
}

}  // namespace

ListOfPublishedDrafts::ListOfPublishedDrafts(Wiki* wiki, live_replication::RecentChangesReader* recentChangesReader,
                                             const string& stateFile, int daysToKeep)
    : m_wiki(wiki), m_recentChangesReader(recentChangesReader), m_stateFile(stateFile), m_daysToKeep(daysToKeep) {}

ListOfPublishedDrafts::Articles ListOfPublishedDrafts::getNewlyPublishedDrafts(json::Value& state) {
  string continueToken = state["rcContinueToken"].str();
  live_replication::RecentLogEventsOptions options;
  options.start = Date::now() - DateDiff(3600 * 36);
  // Start from where we stopped last time. Unless we restart after a long break, this overrides options.start.
  // Note: we could also regenerate the full list every time (m_daysToKeep days). However, the incremental approach
  // allows the manual removal of content if needed. Also, the edit summary is incremental anyway.
  options.continueToken = &continueToken;
  vector<LogEvent> logEvents = m_recentChangesReader->getRecentLogEvents(options);

  Articles newArticles;
  using ArticlesByTitle = unordered_map<string, Article*>;
  ArticlesByTitle articlesByCurrentTitle;
  unordered_set<string> usersToCheck;

  for (const LogEvent& logEvent : logEvents) {
    if (logEvent.type == mwc::LE_MOVE && !logEvent.title.empty() && !logEvent.newTitle().empty()) {
      ArticlesByTitle::iterator articleIt = articlesByCurrentTitle.find(logEvent.title);
      if (articleIt != articlesByCurrentTitle.end()) {
        // An already published draft was moved.
        Article* article = articleIt->second;
        if (m_wiki->getTitleNamespace(logEvent.newTitle()) == NS_MAIN) {
          // It's still in main, keep tracking it.
          article->currentTitle = logEvent.newTitle();
          article->lastMoveDate = logEvent.timestamp;
          articlesByCurrentTitle.erase(articleIt);
          articlesByCurrentTitle[logEvent.newTitle()] = article;
        } else if (logEvent.action == "move_redir") {
          // The article was moved outside of the main namespace without creating a direct, so it can be ignored.
          article->deleted = true;
          articlesByCurrentTitle.erase(articleIt);
        } else {
          // The article was moved outside of the main namespace but there is still a redirect from main pointing to it.
          // This should be fixed by deleting the redirect, so keep the entry in the list.
        }
      } else if (m_wiki->getTitleNamespace(logEvent.title) != NS_MAIN &&
                 m_wiki->getTitleNamespace(logEvent.newTitle()) == NS_MAIN) {
        // New draft published to main.
        newArticles.emplace_back();
        Article& article = newArticles.back();
        article.draftTitle = logEvent.title;
        article.firstTitleInMain = logEvent.newTitle();
        article.currentTitle = logEvent.newTitle();
        article.publisher = logEvent.user;
        article.publishDate = logEvent.timestamp;
        article.lastMoveDate = logEvent.timestamp;
        // The pointer remains valid because newArticles is a list.
        articlesByCurrentTitle[article.currentTitle] = &article;
        usersToCheck.insert(logEvent.user);
      }
    } else if (logEvent.type == mwc::LE_DELETE && logEvent.action == "delete") {
      ArticlesByTitle::iterator articleIt = articlesByCurrentTitle.find(logEvent.title);
      if (articleIt != articlesByCurrentTitle.end()) {
        Article* article = articleIt->second;
        if (logEvent.timestamp > article->lastMoveDate) {
          article->deleted = true;
          articlesByCurrentTitle.erase(articleIt);
        } else {
          // Some other page was overwritten by the last move of the tracked article. The tracked article itself was not
          // deleted (reaching this branch is possible because events can be slightly out of order).
        }
      }
    }
  }

  unordered_set<string> trustedUsers = getTrustedUsers(*m_wiki, usersToCheck);
  newArticles.erase(std::remove_if(newArticles.begin(), newArticles.end(),
                                   [&](const Article& article) {
                                     return article.deleted || trustedUsers.count(article.publisher) != 0;
                                   }),
                    newArticles.end());

  state.getMutable("rcContinueToken") = continueToken;
  return newArticles;
}

string ListOfPublishedDrafts::describeNewArticle(const Article& article) {
  string buffer = "*";
  buffer += DateFormatter::getByLang("fr").format(article.publishDate, DateFormatter::LONG, DateFormatter::MINUTE);
  buffer += " {{u|" + article.publisher + "}} a déplacé la page ";
  buffer += m_wiki->makeLink(article.draftTitle);
  buffer += " vers ";
  buffer += m_wiki->makeLink(article.firstTitleInMain);
  if (article.firstTitleInMain != article.currentTitle) {
    buffer += " (titre actuel : ";
    buffer += m_wiki->makeLink(article.currentTitle);
    buffer += ")";
  }
  return buffer;
}

string ListOfPublishedDrafts::generateEditSummary(const Articles& articles) {
  string summary;
  int remainingPages = articles.size();
  for (const Article& article : articles) {
    const string& newArticle = article.currentTitle;
    if (!summary.empty()) summary += ", ";
    int newLength = summary.size() + newArticle.size();
    if (newLength >= 400) {
      summary += std::to_string(remainingPages);
      summary += remainingPages == 1 ? " autre page" : " autres pages";
      break;
    }
    summary += m_wiki->makeLink(newArticle);
    remainingPages--;
  }
  return summary;
}

void ListOfPublishedDrafts::updateBotSection(const Articles& newArticles, bool dryRun) {
  if (newArticles.empty()) {
    CBL_INFO << "No new articles created by moving drafts since the last run";
    return;
  }

  mwc::WriteToken writeToken;
  string code = m_wiki->readPageContentIfExists(LIST_TITLE, &writeToken);

  string_view oldBotSection = mwc::readBotSection(code);
  EventsByDay eventsByDay;
  eventsByDay.addEventsFromCode(oldBotSection);
  for (const Article& article : newArticles) {
    eventsByDay.addEvent(article.publishDate, describeNewArticle(article));
  }
  eventsByDay.removeOldEvents(m_daysToKeep);
  mwc::replaceBotSection(code, eventsByDay.toString());

  string comment = generateEditSummary(newArticles);

  if (dryRun) {
    CBL_INFO << "[DRY RUN] comment=" << comment << "\n" << code;
  } else {
    m_wiki->writePage(LIST_TITLE, code, writeToken, comment, mwc::EDIT_MINOR);
  }
}

void ListOfPublishedDrafts::update(bool dryRun) {
  json::Value state = loadState(m_stateFile);
  Articles articles = getNewlyPublishedDrafts(state);
  updateBotSection(articles, dryRun);
  if (!dryRun) {
    saveState(m_stateFile, state);
  }
}
