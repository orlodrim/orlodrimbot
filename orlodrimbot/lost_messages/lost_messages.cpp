#include <string>
#include "cbl/args_parser.h"
#include "cbl/file.h"
#include "cbl/json.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"
#include "lost_messages_lib.h"

using std::string;

int main(int argc, char** argv) {
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
  string mentorStateFile;
  string rcStateFile;
  string rcDatabaseFile;
  bool dryRun = false;
  cbl::parseArgs(argc, argv, &wikiFlags, "--mentorstate", &mentorStateFile, "--rcstate", &rcStateFile,
                 "--rcdatabasefile", &rcDatabaseFile, "--dryrun", &dryRun);
  mwc::Wiki wiki;
  mwc::initWikiFromFlags(wikiFlags, wiki);

  json::Value rcState;
  if (cbl::fileExists(rcStateFile)) {
    rcState = json::parse(cbl::readFile(rcStateFile));
  }
  live_replication::RecentChangesReader recentChangesReader(rcDatabaseFile);

  LostMessages lostMessages(&wiki, mentorStateFile);
  lostMessages.runOnRecentChanges(recentChangesReader, rcState, dryRun);

  if (!dryRun) {
    cbl::writeFileAtomically(rcStateFile, rcState.toJSON(json::INDENTED) + "\n");
  }
  return 0;
}
