#include "copy_page.h"
#include <string>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "cbl/file.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/request.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::Revision;
using mwc::Wiki;
using std::string;
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
std::pair<Date, string> getMostRecentChange(Wiki& wiki, const vector<string>& pages) {
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

void copyPageIfTemplatesAreUnchanged(Wiki& wiki, live_replication::RecentChangesReader* recentChangesReader,
                                     const string& stateFile, const string& sourcePage, const string& targetPage) {
  json::Value state;
  if (cbl::fileExists(stateFile)) {
    state = json::parse(cbl::readFile(stateFile));
  }
  json::Value& pageState = state.getMutable("pages").getMutable(sourcePage);
  mwc::revid_t lastProcessedRevid = pageState.has("revid") ? pageState["revid"].numberAsInt64() : -1;

  if (recentChangesReader) {
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

  Revision revision = wiki.readPage(sourcePage, mwc::RP_REVID | mwc::RP_TIMESTAMP | mwc::RP_CONTENT);
  if (revision.revid == lastProcessedRevid) {
    CBL_INFO << "No change on '" << sourcePage << "' since last run";
    return;
  } else if (Date::now() - revision.timestamp < DateDiff::fromMinutes(2)) {
    // Give users a few minutes to check their own edits.
    CBL_INFO << "The page '" << sourcePage << "' was modified less than 2 minutes ago";
    return;
  }

  vector<string> templates = getTemplates(wiki, sourcePage);
  auto [mostRecentChange, affectedPage] = getMostRecentChange(wiki, templates);
  if (mostRecentChange >= revision.timestamp) {
    throw CopyError(
        cbl::concat("Le modèle récemment modifié [[:", affectedPage, "]] est inclus dans [[", sourcePage, "]]"));
  }
  string newContent = cbl::concat("<!-- Cette page est mise à jour automatiquement à partir de [[", sourcePage,
                                  "]]. Les changements faits directement ici seront écrasés. -->\n", revision.content);
  CBL_INFO << "Updating '" << targetPage << "' from '" << sourcePage << "'";
  wiki.writePage(targetPage, newContent, mwc::WriteToken::newWithoutConflictDetection(),
                 cbl::concat("Mise à jour à partir de [[", sourcePage, "]]"));

  pageState.getMutable("revid") = revision.revid;
  cbl::writeFile(stateFile, state.toJSON(json::INDENTED) + "\n");
}
