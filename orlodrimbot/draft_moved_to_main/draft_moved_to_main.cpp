// Creates or updates a list of pages moved to the main namespace from somewhere else.
// This is used to detect drafts published by moving the page.
#include <algorithm>
#include <string>
#include "cbl/args_parser.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"
#include "draft_moved_to_main_lib.h"

using std::string;

int main(int argc, char** argv) {
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
  string stateFile;
  string rcDatabaseFile;
  int daysToKeep = 14;
  bool dryRun = false;
  cbl::parseArgs(argc, argv, &wikiFlags, "--statefile,required", &stateFile, "--rcdatabasefile,required",
                 &rcDatabaseFile, "--daystokeep", &daysToKeep, "--dryrun", &dryRun);
  daysToKeep = std::min(std::max(daysToKeep, 1), 14);
  mwc::Wiki wiki;
  mwc::initWikiFromFlags(wikiFlags, wiki);
  live_replication::RecentChangesReader recentChangesReader(rcDatabaseFile);
  ListOfPublishedDrafts listOfPublishedDrafts(&wiki, &recentChangesReader, stateFile, daysToKeep);
  listOfPublishedDrafts.update(dryRun);
  return 0;
}
