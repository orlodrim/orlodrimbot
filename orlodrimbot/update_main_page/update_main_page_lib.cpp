#include "update_main_page_lib.h"
#include <re2/re2.h>
#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "cbl/error.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "cbl/unicode_fr.h"
#include "mwclient/parser.h"
#include "mwclient/util/bot_section.h"
#include "mwclient/util/include_tags.h"
#include "mwclient/util/templates_by_name.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"
#include "orlodrimbot/wikiutil/date_formatter.h"
#include "orlodrimbot/wikiutil/date_parser.h"
#include "orlodrimbot/wikiutil/wiki_local_time.h"
#include "template_expansion_cache.h"

using cbl::Date;
using cbl::DateDiff;
using live_replication::RecentChangesReader;
using mwc::PageProtection;
using mwc::Revision;
using mwc::Wiki;
using std::pair;
using std::string;
using std::string_view;
using std::unordered_map;
using std::unordered_set;
using std::vector;
using wikicode::getTemplatesByName;
using wikiutil::DateFormatter;
using wikiutil::DateParser;

namespace {

constexpr string_view PICTURE_OF_THE_DAY_PREFIX = "Wikipédia:Image du jour/";
constexpr string_view ANNIVERSARIES_PREFIX = "Wikipédia:Éphéméride/";
constexpr string_view FEATURED_ARTICLE_PREFIX = "Wikipédia:Lumière sur/";
constexpr string_view SPECIAL_BLANK_SOURCE_PAGE = "Special:BLANK_PAGE";

class RetryLaterError : public cbl::Error {
public:
  using Error::Error;
};

class ReportableError : public cbl::Error {
public:
  using Error::Error;
};

// Return the day in the time zone used to select the picture of the day and other pages changing daily.
// Currently, the time zone is UTC, so no shift is needed.
Date getDisplayedDay(const Date& now) {
  return wikiutil::getFrWikiLocalTime(now).extractDay();
}

string getFormattedDay(const Date& day, DateFormatter::Format dateFormat) {
  return DateFormatter::getByLang("fr").format(day, dateFormat);
}

string getPictureOfTheDayPage(const Date& day) {
  return cbl::concat(PICTURE_OF_THE_DAY_PREFIX, getFormattedDay(day, DateFormatter::LONG));
}

string getAnniversariesPage(const Date& day) {
  string today = getFormattedDay(day, DateFormatter::LONG_1ST);
  size_t lastSpace = today.rfind(" ");
  CBL_ASSERT(lastSpace != string::npos);
  return cbl::concat(ANNIVERSARIES_PREFIX, string_view(today).substr(0, lastSpace));
}

class SourceTargetMap {
public:
  explicit SourceTargetMap(const vector<string>& featuredArticles, const Date& displayedDay)
      : m_sourceToTarget({
            {"Modèle:Accueil actualité", "Modèle:Accueil actualité/Copie sans modèles"},
            {"Wikipédia:Le saviez-vous ?/Anecdotes sur l'accueil",
             "Wikipédia:Le saviez-vous ?/Anecdotes sur l'accueil/Copie sans modèles"},
            {getPictureOfTheDayPage(displayedDay), "Wikipédia:Accueil principal/Image du jour (copie sans modèles)"},
            {getAnniversariesPage(displayedDay), "Wikipédia:Accueil principal/Éphéméride (copie sans modèles)"},
        }) {
    if (!featuredArticles.empty()) {
      m_sourceToTarget.insert({cbl::concat(FEATURED_ARTICLE_PREFIX, featuredArticles[0]),
                               "Wikipédia:Accueil principal/Lumière sur (copie sans modèles)"});
      string optionalSecondPage;
      if (featuredArticles.size() >= 2) {
        optionalSecondPage = cbl::concat(FEATURED_ARTICLE_PREFIX, featuredArticles[1]);
      } else {
        optionalSecondPage = SPECIAL_BLANK_SOURCE_PAGE;
      }
      m_sourceToTarget.insert({optionalSecondPage, "Wikipédia:Accueil principal/Lumière sur 2 (copie sans modèles)"});
    }
    for (const auto& [source, target] : m_sourceToTarget) {
      m_targetToSource[target] = source;
    }
  }

  const string* getTargetFromSource(const string& sourcePage) const {
    unordered_map<string, string>::const_iterator it = m_sourceToTarget.find(sourcePage);
    return it == m_sourceToTarget.end() ? nullptr : &it->second;
  }

  const string* getSourceFromTarget(const string& targetPage) const {
    unordered_map<string, string>::const_iterator it = m_targetToSource.find(targetPage);
    return it == m_targetToSource.end() ? nullptr : &it->second;
  }

private:
  unordered_map<string, string> m_sourceToTarget;
  unordered_map<string, string> m_targetToSource;
};

// Processing the most recent update first gives some robustness against unhandled errors affecting a specific page.
class PageStack {
public:
  PageStack() = default;
  explicit PageStack(const json::Value& jsonVector) {
    m_pagesVec.reserve(jsonVector.array().size());
    for (const json::Value& value : jsonVector.array()) {
      push(value.str());
    }
  }

  bool empty() const { return m_pagesVec.empty(); }
  const string& top() const { return m_pagesVec.back(); }
  void push(const string& page) {
    if (m_pagesSet.insert(page).second) {
      m_pagesVec.push_back(page);
    }
  }
  void pop() {
    CBL_ASSERT(!m_pagesVec.empty());
    m_pagesSet.erase(m_pagesVec.back());
    m_pagesVec.pop_back();
  }
  void markTopPageAsFailed() { m_pagesWithError.push_back(top()); }

  json::Value toJSON() const {
    json::Value result;
    result.setToEmptyArray();
    for (const string& page : m_pagesWithError) {
      result.addItem() = page;
    }
    for (const string& page : m_pagesVec) {
      result.addItem() = page;
    }
    return result;
  }

private:
  vector<string> m_pagesVec;
  unordered_set<string> m_pagesSet;
  vector<string> m_pagesWithError;
};

// Decides whether we should cache the current rendering of a page that just changed, because it might be displayed
// later on the home page.
// For instance, if "Wikipédia:Éphéméride/22 novembre" is modified on November 20 and a template on it modified on
// November 21 at 23:55 UTC, we will have a cached version from November 20, so we don't have to re-render the page on
// November 22 and take the risk of using a template that might have been broken 5 minutes before.
bool shouldCachePage(const string& sourcePage, const Date& displayedDay) {
  if (sourcePage.starts_with(PICTURE_OF_THE_DAY_PREFIX)) {
    const DateParser& dateParser = DateParser::getByLang("fr");
    Date day = dateParser.parseDate(string_view(sourcePage).substr(PICTURE_OF_THE_DAY_PREFIX.size()), 0);
    return !day.isNull() && day >= displayedDay;
  } else if (sourcePage.starts_with(ANNIVERSARIES_PREFIX) || sourcePage.starts_with(FEATURED_ARTICLE_PREFIX)) {
    return true;
  }
  return false;
}

vector<string> getStylesheets(Wiki& wiki, const string& code) {
  static const re2::RE2 reSource(R"re( src="([^"]*)")re");
  wikicode::List parsedCode = wikicode::parse(code);
  vector<string> stylesheets;
  for (wikicode::Tag& tag : parsedCode.getTags()) {
    if (tag.tagName() != "templatestyles") continue;
    string source;
    if (RE2::PartialMatch(tag.openingTag(), reSource, &source)) {
      stylesheets.push_back(wiki.normalizeTitle(source, mwc::NS_TEMPLATE));
    } else {
      CBL_ERROR << "Cannot extract source from <templatestyles> tag: " << tag.openingTag();
    }
  }
  std::sort(stylesheets.begin(), stylesheets.end());
  stylesheets.erase(std::unique(stylesheets.begin(), stylesheets.end()), stylesheets.end());
  return stylesheets;
}

void checkStylesheetsProtection(Wiki& wiki, const string& expandedCode) {
  vector<string> stylesheets = getStylesheets(wiki, expandedCode);
  if (stylesheets.empty()) {
    return;
  }
  unordered_map<string, vector<PageProtection>> pagesProtections = wiki.getPagesProtections(stylesheets);
  vector<string> errorsVector;
  for (const auto& [title, protections] : pagesProtections) {
    const PageProtection* editProtection = nullptr;
    for (const PageProtection& protection : protections) {
      if (protection.type == mwc::PRT_EDIT) {
        editProtection = &protection;
        break;
      }
    }
    if (!editProtection) {
      errorsVector.push_back(cbl::concat("la feuille de style ", wiki.makeLink(title), " n'est pas protégée"));
    } else if (editProtection->level != mwc::PRL_SYSOP && editProtection->level != mwc::PRL_AUTOPATROLLED) {
      errorsVector.push_back(cbl::concat("la feuille de style ", wiki.makeLink(title),
                                         " a un niveau de protection inférieur à « semi-protection étendue »"));
    } else if (!editProtection->expiry.isNull() && editProtection->expiry < Date::now() + DateDiff::fromDays(3)) {
      errorsVector.push_back(
          cbl::concat("la protection de la feuille de style ", wiki.makeLink(title), " expire dans moins de 3 jours"));
    }
  }
  for (const string& title : stylesheets) {
    if (pagesProtections.count(title) == 0) {
      errorsVector.push_back(cbl::concat("impossible de vérifier la protection de ", wiki.makeLink(title)));
    }
  }
  if (!errorsVector.empty()) {
    throw ReportableError(cbl::join(errorsVector, ", "));
  }
}

string joinErrors(const vector<string>& errors) {
  string code;
  for (const string& error : errors) {
    cbl::append(code, "* ", error, "\n");
  }
  return code;
}

vector<string> jsonToStringVector(const json::Value& jsonArray) {
  vector<string> strings;
  for (const json::Value& value : jsonArray.array()) {
    strings.push_back(value.str());
  }
  return strings;
}

json::Value stringVectorToJson(const vector<string>& strings) {
  json::Value jsonArray;
  jsonArray.setToEmptyArray();
  for (const string& value : strings) {
    jsonArray.addItem() = value;
  }
  return jsonArray;
}

class MainPageUpdater {
public:
  MainPageUpdater(Wiki& wiki, json::Value& state, RecentChangesReader& recentChangesReader,
                  TemplateExpansionCache& templateExpansionCache);
  ~MainPageUpdater();
  void run();

private:
  void updatePendingWork(const SourceTargetMap& sourceTargetMap, const Date& now, const Date& displayedDay,
                         bool featuredArticlesUpdated);
  pair<Revision, ExpansionResult> readAndCacheSource(const string& sourcePage, int maxSizeToExpand = -1);
  bool readFeaturedArticles(const Date& day, vector<string>& featuredArticles, vector<string>& errors);
  bool updateTargetPage(const string& targetPage, const SourceTargetMap& sourceTargetMap, const Date& displayedDay,
                        vector<string>& errors, bool& canClearErrorLog);

  Wiki& m_wiki;
  json::Value& m_state;
  RecentChangesReader& m_recentChangesReader;
  TemplateExpansionCache& m_templateExpansionCache;

  string m_rcToken;
  Date m_updateTimestamp;
  Date m_featuredArticlesDay;
  vector<string> m_featuredArticles;
  PageStack m_sourcesToCache;
  PageStack m_targetsToUpdate;
  string m_reportedErrors;
};

MainPageUpdater::MainPageUpdater(Wiki& wiki, json::Value& state, RecentChangesReader& recentChangesReader,
                                 TemplateExpansionCache& templateExpansionCache)
    : m_wiki(wiki), m_state(state), m_recentChangesReader(recentChangesReader),
      m_templateExpansionCache(templateExpansionCache), m_rcToken(state["rc_token"].str()),
      m_updateTimestamp(Date::fromISO8601OrEmpty(state["update_timestamp"].str())),
      m_featuredArticlesDay(Date::fromISO8601OrEmpty(state["featured_articles_day"].str())),
      m_featuredArticles(jsonToStringVector(state["featured_articles"])), m_sourcesToCache(state["sources_to_cache"]),
      m_targetsToUpdate(state["targets_to_update"]), m_reportedErrors(state["reported_errors"].str()) {}

MainPageUpdater::~MainPageUpdater() {
  m_state.getMutable("rc_token") = m_rcToken;
  m_state.getMutable("update_timestamp") = m_updateTimestamp.isNull() ? "" : m_updateTimestamp.toISO8601();
  m_state.getMutable("featured_articles_day") = m_featuredArticlesDay.isNull() ? "" : m_featuredArticlesDay.toISO8601();
  m_state.getMutable("featured_articles") = stringVectorToJson(m_featuredArticles);
  m_state.getMutable("sources_to_cache") = m_sourcesToCache.toJSON();
  m_state.getMutable("targets_to_update") = m_targetsToUpdate.toJSON();
  m_state.getMutable("reported_errors") = m_reportedErrors;
}

void MainPageUpdater::updatePendingWork(const SourceTargetMap& sourceTargetMap, const Date& now,
                                        const Date& displayedDay, bool featuredArticlesUpdated) {
  m_recentChangesReader.enumRecentChanges({.continueToken = &m_rcToken}, [&](const mwc::RecentChange& rc) {
    const string& sourcePage = rc.title();
    const string* targetPage = sourceTargetMap.getTargetFromSource(sourcePage);
    if (targetPage) {
      m_targetsToUpdate.push(*targetPage);
    } else if (shouldCachePage(sourcePage, displayedDay)) {
      m_sourcesToCache.push(sourcePage);
    }
  });

  if (getDisplayedDay(m_updateTimestamp) != displayedDay) {
    m_targetsToUpdate.push("Wikipédia:Accueil principal/Image du jour (copie sans modèles)");
    m_targetsToUpdate.push("Wikipédia:Accueil principal/Éphéméride (copie sans modèles)");
  }
  m_updateTimestamp = now;

  if (featuredArticlesUpdated) {
    m_targetsToUpdate.push("Wikipédia:Accueil principal/Lumière sur (copie sans modèles)");
    m_targetsToUpdate.push("Wikipédia:Accueil principal/Lumière sur 2 (copie sans modèles)");
  }
}

pair<Revision, ExpansionResult> MainPageUpdater::readAndCacheSource(const string& sourcePage, int maxSizeToExpand) {
  CBL_INFO << "Reading '" << sourcePage << "'";
  Revision sourceRev = m_wiki.readPage(sourcePage, mwc::RP_REVID | mwc::RP_TIMESTAMP | mwc::RP_CONTENT | mwc::RP_USER);
  string transcludedCode;
  mwc::include_tags::parse(sourceRev.content, nullptr, &transcludedCode);
  if (maxSizeToExpand != -1 && static_cast<int>(transcludedCode.size()) > maxSizeToExpand) {
    throw ReportableError(
        cbl::concat("la page source est trop longue (plus de ", std::to_string(maxSizeToExpand / 1000), " Ko)"));
  }
  ExpansionResult expansion = m_templateExpansionCache.expand(transcludedCode, sourcePage, sourceRev.revid);
  return {sourceRev, expansion};
}

bool MainPageUpdater::readFeaturedArticles(const Date& day, vector<string>& featuredArticles, vector<string>& errors) {
  string month = unicode_fr::capitalize(DateFormatter::getByLang("fr").getMonthName(day.month()));
  string sourcePage = cbl::concat("Wikipédia:Lumière sur/", month, " ", std::to_string(day.year()));
  try {
    string content;
    try {
      content = m_wiki.readPageContent(sourcePage);
    } catch (const mwc::PageNotFoundError& error) {
      throw ReportableError("la page n'existe pas");
    }
    wikicode::List parsedCode = wikicode::parse(content);
    for (const wikicode::Template& template_ : getTemplatesByName(m_wiki, parsedCode, "Lumière sur/Accueil")) {
      wikicode::ParsedFields fields = template_.getParsedFields();
      string dayOfMonthStr = (day.day() < 10 ? "0" : "") + std::to_string(day.day());
      vector<string> newFeaturedArticles;
      for (string_view suffix : {"a", "b"}) {
        string param = cbl::concat(dayOfMonthStr, suffix);
        const string& article = fields[cbl::concat(dayOfMonthStr, suffix)];
        if (!article.empty()) {
          if (m_wiki.getTitleNamespace(article) != mwc::NS_MAIN) {
            throw ReportableError(cbl::concat(m_wiki.makeLink(article), " n'est pas une page de l'espace principal"));
          }
          newFeaturedArticles.push_back(article);
        }
      }
      if (newFeaturedArticles.empty()) {
        throw ReportableError("aucun article n'est renseigné pour aujourd'hui");
      }
      featuredArticles = std::move(newFeaturedArticles);
      return true;
    }
    throw ReportableError("le modèle {{m|Lumière sur/Accueil}} n'a pas été trouvé dans la page");
  } catch (const ReportableError& error) {
    string errorMessage = cbl::concat("Impossible de lire les articles mis en lumière du jour à partir de [[",
                                      sourcePage, "]] : ", error.what());
    errors.push_back(errorMessage);
    CBL_ERROR << errorMessage;
  } catch (const mwc::WikiError& error) {
    CBL_ERROR << error.what();
  }
  return false;
}

bool MainPageUpdater::updateTargetPage(const string& targetPage, const SourceTargetMap& sourceTargetMap,
                                       const Date& displayedDay, vector<string>& errors, bool& canClearErrorLog) {
  const string* sourcePage = sourceTargetMap.getSourceFromTarget(targetPage);
  if (!sourcePage) {
    return true;
  }
  Date now = Date::now();

  try {
    string newCode;
    if (*sourcePage == SPECIAL_BLANK_SOURCE_PAGE) {
      newCode = "<!-- Pas de second article mis en lumière aujourd'hui -->";
    } else {
      Revision sourceRev;
      ExpansionResult expansion;
      try {
        std::tie(sourceRev, expansion) = readAndCacheSource(*sourcePage, 25'000);
      } catch (const mwc::PageNotFoundError&) {
        throw ReportableError("la page source n'existe pas");
      }
      if (m_wiki.readRedirect(sourceRev.content, nullptr, nullptr)) {
        throw ReportableError("la page source est une redirection");
      }
      if (now - sourceRev.timestamp < DateDiff::fromMinutes(2) && sourceRev.user != "GhosterBot") {
        // Give users a few minutes to check their own edits.
        throw RetryLaterError(cbl::concat("The page '", *sourcePage, "' was modified less than 2 minutes ago"));
      } else if (expansion.lastChangedTemplateTimestamp >=
                 std::max(sourceRev.timestamp, now - DateDiff::fromHours(1))) {
        throw ReportableError(cbl::concat("le modèle récemment modifié ",
                                          m_wiki.makeLink(expansion.lastChangedTemplate), " est inclus dans ",
                                          m_wiki.makeLink(*sourcePage)));
      }
      newCode = std::move(expansion.code);
      checkStylesheetsProtection(m_wiki, newCode);

      if (targetPage == "Wikipédia:Accueil principal/Éphéméride (copie sans modèles)") {
        string frame = m_wiki.expandTemplates(cbl::concat(
            "{{Wikipédia:Accueil principal/Cadre éphéméride|jour=", std::to_string(displayedDay.day()),
            "|mois=", DateFormatter::getByLang("fr").getMonthName(displayedDay.month()), "|contenu=PLACEHOLDER}}"));
        newCode = cbl::replace(frame, "PLACEHOLDER", newCode);
      }
    }
    CBL_INFO << "Updating '" << targetPage << "' from '" << *sourcePage << "'";
    if (!mwc::replaceBotSectionInPage(m_wiki, targetPage, newCode,
                                      cbl::concat("Mise à jour à partir de [[", *sourcePage, "]]"),
                                      mwc::BS_MUST_EXIST)) {
      throw ReportableError(cbl::concat("section de bot non trouvée sur [[", targetPage, "]]"));
    }
    return true;
  } catch (const RetryLaterError& error) {
    CBL_INFO << error.what();
    // If something triggers an error every time we try to update a page, do not temporarily clear the error log every
    // time we skip the update because the page was temporarily modified.
    canClearErrorLog = false;
  } catch (const ReportableError& error) {
    string errorMessage =
        cbl::concat("Erreur lors de la copie de [[", *sourcePage, "]] vers [[", targetPage, "]] : ", error.what());
    errors.push_back(errorMessage);
    CBL_ERROR << errorMessage;
  } catch (const mwc::WikiError& error) {
    CBL_ERROR << error.what();
  }
  return false;
}

void MainPageUpdater::run() {
  vector<string> errors;
  bool canClearErrorLog = true;

  Date now = Date::now();
  Date displayedDay = getDisplayedDay(now);

  bool featuredArticlesUpdated = false;
  if (m_featuredArticlesDay != displayedDay) {
    if (readFeaturedArticles(displayedDay, m_featuredArticles, errors)) {
      m_featuredArticlesDay = displayedDay;
      featuredArticlesUpdated = true;
    }
  }
  SourceTargetMap sourceTargetMap(m_featuredArticles, displayedDay);
  updatePendingWork(sourceTargetMap, now, displayedDay, featuredArticlesUpdated);

  for (; !m_targetsToUpdate.empty(); m_targetsToUpdate.pop()) {
    const string& targetPage = m_targetsToUpdate.top();
    if (!updateTargetPage(targetPage, sourceTargetMap, displayedDay, errors, canClearErrorLog)) {
      m_targetsToUpdate.markTopPageAsFailed();
    }
  }

  for (; !m_sourcesToCache.empty(); m_sourcesToCache.pop()) {
    const string& sourcePage = m_sourcesToCache.top();
    try {
      readAndCacheSource(sourcePage);
    } catch (const mwc::PageNotFoundError& error) {
      CBL_WARNING << error.what();
    } catch (const mwc::WikiError& error) {
      CBL_ERROR << error.what();
      m_sourcesToCache.markTopPageAsFailed();
    }
  }

  string joinedErrors = joinErrors(errors);
  if (joinedErrors != m_reportedErrors && (canClearErrorLog || !errors.empty())) {
    string report = joinedErrors.empty() ? "<!-- Aucune erreur -->" : joinedErrors;
    m_wiki.writePage("Utilisateur:OrlodrimBot/Statut page d'accueil", report,
                     mwc::WriteToken::newWithoutConflictDetection(), "Rapport d'erreur");
    m_reportedErrors = joinedErrors;
  }
}

}  // namespace

void updateMainPage(Wiki& wiki, json::Value& state, RecentChangesReader& recentChangesReader,
                    TemplateExpansionCache& templateExpansionCache) {
  MainPageUpdater updater(wiki, state, recentChangesReader, templateExpansionCache);
  updater.run();
}
