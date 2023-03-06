#include "copy_page.h"
#include <re2/re2.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "cbl/error.h"
#include "cbl/file.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/parser.h"
#include "mwclient/request.h"
#include "mwclient/util/bot_section.h"
#include "mwclient/util/include_tags.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::PageProtection;
using mwc::Revision;
using mwc::Wiki;
using std::pair;
using std::string;
using std::unordered_map;
using std::vector;

// Computes the templates used when transcluding `page` on the main page (except `page` itself).
vector<string> getTemplates(Wiki& wiki, const string& page) {
  mwc::WikiRequest request("parse");
  request.setParam("title", "Wikipédia:Accueil principal");
  request.setParam("text", cbl::concat("{{", page, "}}"));
  request.setParam("prop", "templates");
  json::Value answer = request.run(wiki);

  vector<string> templates;
  for (const json::Value& value : answer["parse"]["templates"].array()) {
    const string& template_ = value["*"].str();
    if (template_ != page) {
      templates.push_back(value["*"].str());
    }
  }
  return templates;
}

// Finds the most recent change done on any page in `pages`.
pair<Date, string> getMostRecentChange(Wiki& wiki, const vector<string>& pages) {
  vector<Revision> revisions;
  revisions.reserve(pages.size());
  for (const string& page : pages) {
    revisions.emplace_back().title = page;
  }
  wiki.readPages(mwc::RP_TIMESTAMP, revisions);
  Date mostRecentChange;
  string affectedPage;
  for (const Revision& revision : revisions) {
    if (revision.revid >= 0 && revision.timestamp > mostRecentChange) {
      mostRecentChange = revision.timestamp;
      affectedPage = revision.title;
    }
  }
  return {mostRecentChange, affectedPage};
}

vector<string> getStylesheets(Wiki& wiki, const string& code) {
  static const re2::RE2 reSource(R"re( src="([^"]*)")re");
  wikicode::List parsedCode = wikicode::parse(code);
  vector<string> stylesheets;
  for (wikicode::Tag& tag : parsedCode.getTags()) {
    if (tag.tagName() != "templatestyles") continue;
    string source;
    RE2::PartialMatch(tag.openingTag(), reSource, &source);
    stylesheets.push_back(wiki.normalizeTitle(source));
  }
  std::sort(stylesheets.begin(), stylesheets.end());
  stylesheets.erase(std::unique(stylesheets.begin(), stylesheets.end()), stylesheets.end());
  return stylesheets;
}

void checkStylesheetsProtection(Wiki& wiki, const string& expandedCode) {
  vector<string> stylesheets = getStylesheets(wiki, expandedCode);
  unordered_map<string, vector<PageProtection>> pagesProtections;
  if (!stylesheets.empty()) {
    pagesProtections = wiki.getPagesProtections(stylesheets);
  }
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
    throw CopyError(cbl::join(errorsVector, ", "));
  }
}

void copyPageIfTemplatesAreUnchanged(Wiki& wiki, live_replication::RecentChangesReader* recentChangesReader,
                                     const string& stateFile, const string& sourcePage, const string& targetPage) {
  json::Value state;
  if (!stateFile.empty() && cbl::fileExists(stateFile)) {
    state = json::parse(cbl::readFile(stateFile));
  }
  json::Value& pageState = state.getMutable("pages").getMutable(sourcePage);
  mwc::revid_t lastProcessedRevid = pageState.has("revid") ? pageState["revid"].numberAsInt64() : -1;

  if (recentChangesReader && !pageState["pendingchange"].boolean()) {
    bool pageChanged = false;
    recentChangesReader->enumRecentChanges({.type = mwc::RC_EDIT | mwc::RC_NEW,
                                            .properties = mwc::RP_TITLE | mwc::RP_REVID,
                                            .start = Date::now() - DateDiff::fromMinutes(15)},
                                           [&](const mwc::RecentChange& change) {
                                             if (change.title() == sourcePage &&
                                                 (change.type() == mwc::RC_EDIT || change.type() == mwc::RC_NEW) &&
                                                 change.revision().revid > lastProcessedRevid) {
                                               pageChanged = true;
                                             }
                                           });
    if (!pageChanged) {
      CBL_INFO << "No change on '" << sourcePage << "' since last run";
      return;
    }
  }

  cbl::RunOnDestroy saveState([&] {
    if (!stateFile.empty()) {
      cbl::writeFile(stateFile, state.toJSON(json::INDENTED) + "\n");
    }
  });
  pageState.getMutable("pendingchange") = true;

  Revision revision = wiki.readPage(sourcePage, mwc::RP_REVID | mwc::RP_TIMESTAMP | mwc::RP_CONTENT | mwc::RP_USER);
  if (revision.revid == lastProcessedRevid) {
    CBL_INFO << "No change on '" << sourcePage << "' since last run";
    return;
  } else if (Date::now() - revision.timestamp < DateDiff::fromMinutes(2) && revision.user != "GhosterBot") {
    // Give users a few minutes to check their own edits.
    CBL_INFO << "The page '" << sourcePage << "' was modified less than 2 minutes ago";
    return;
  }

  string transcludedCode;
  mwc::include_tags::parse(revision.content, nullptr, &transcludedCode);
  string expandedCode = wiki.expandTemplates(transcludedCode, sourcePage, revision.revid);
  checkStylesheetsProtection(wiki, expandedCode);

  vector<string> templates = getTemplates(wiki, sourcePage);
  auto [mostRecentChange, affectedPage] = getMostRecentChange(wiki, templates);
  if (mostRecentChange >= revision.timestamp) {
    throw CopyError(
        cbl::concat("Le modèle récemment modifié [[:", affectedPage, "]] est inclus dans [[", sourcePage, "]]"));
  }
  string newContent = cbl::concat("<!-- Cette page est mise à jour automatiquement à partir de [[", sourcePage,
                                  "]]. Les changements faits directement ici seront écrasés. -->\n", revision.content);
  string docSuffix = "<noinclude>{{Documentation}}</noinclude>";
  if (cbl::endsWith(newContent, docSuffix)) {
    newContent.resize(newContent.size() - docSuffix.size());
  }
  CBL_INFO << "Updating '" << targetPage << "' from '" << sourcePage << "'";
  if (!mwc::replaceBotSectionInPage(wiki, targetPage, expandedCode,
                                    cbl::concat("Mise à jour à partir de [[", sourcePage, "]]"), mwc::BS_MUST_EXIST)) {
    throw CopyError(cbl::concat("Section de bot non trouvée sur [[", targetPage, "]]"));
  }
  pageState.erase("pendingchange");
  pageState.getMutable("revid") = revision.revid;
}
