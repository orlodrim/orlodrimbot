// Replicates recent changes, categories, and the list of existing pages to local databases.
#include <string>
#include "cbl/args_parser.h"
#include "cbl/path.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/wiki.h"
#include "recent_changes_reader.h"
#include "recent_changes_sync.h"

using std::string;

int main(int argc, char** argv) {
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
  bool updateRC = false;
  bool updateCat = false;
  bool updateTitles = false;
  string dataDir;
  cbl::parseArgs(argc, argv, &wikiFlags, "--updaterc", &updateRC, "--updatecat", &updateCat, "--updatetitles",
                 &updateTitles, "--datadir", &dataDir);
  if (!updateRC && !updateCat && !updateTitles) {
    updateRC = true;
    updateCat = true;
    updateTitles = true;
  }
  mwc::Wiki wiki;
  mwc::initWikiFromFlags(wikiFlags, wiki);
  string rcDatabaseFile = cbl::joinPaths(dataDir, "recentchanges.sqlite");
  if (updateRC) {
    live_replication::RecentChangesSync recentChangesSync(rcDatabaseFile);
    recentChangesSync.setSecondsToIgnore(20);
    recentChangesSync.updateDatabaseFromWiki(wiki);
  }
  if (updateCat || updateTitles) {
    live_replication::RecentChangesReader recentChangesReader(rcDatabaseFile);
    // Code not published yet.
  }
  return 0;
}
