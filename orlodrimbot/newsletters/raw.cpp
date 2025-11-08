#include <iostream>
#include <memory>
#include <string>
#include "cbl/args_parser.h"
#include "cbl/log.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"
#include "emergency_stop.h"
#include "raw_lib.h"

using std::string;
using std::unique_ptr;

int main(int argc, char** argv) {
  mwc::Wiki wiki;
  string stateFile;
  string rcDatabaseFile;
  bool force = false;
  bool dryRun = false;
  bool showHelp = false;
  string issue;
  string fromPage;
  string singlePage;
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
  cbl::parseArgs(argc, argv, &wikiFlags, "--statefile,required", &stateFile, "--rcdatabasefile", &rcDatabaseFile,
                 "--issue", &issue, "--dryrun", &dryRun, "--force", &force, "--from", &fromPage, "--page", &singlePage,
                 "--help", &showHelp);
  if (showHelp) {
    std::cerr << "Command line parameters:\n"
              << " --statefile <file>        Path of the file that stores the state of this program (required).\n"
              << " --rcdatabasefile <file>   Path of the local database of recent changes (required unless --issue is "
              << "set).\n"
              << " --issue <str>             Current issue.\n"
              << " --force                   Do not check if the provided issue exists.\n"
              << " --dryrun                  Do not edit any page, just print what would be done.\n"
              << " --from <page>             Starts from this page in the list.\n"
              << " --page <page>             Send to a single page (that must be in the list).\n";
    return 0;
  }
  CBL_ASSERT(!(rcDatabaseFile.empty() && issue.empty()))
      << "Parameter --rcdatabasefile is required unless --issue is specified";

  wiki.setDelayBetweenEdits(10);
  mwc::initWikiFromFlags(wikiFlags, wiki);
  AdvancedUsersEmergencyStopTest emergencyStopTest(wiki);
  wiki.setEmergencyStopTest(std::bind(&AdvancedUsersEmergencyStopTest::isEmergencyStopTriggered, &emergencyStopTest));
  unique_ptr<live_replication::RecentChangesReader> recentChangesReader;
  if (rcDatabaseFile.empty()) {
    recentChangesReader = std::make_unique<live_replication::EmptyRecentChangesReader>();
  } else {
    recentChangesReader = std::make_unique<live_replication::RecentChangesReader>(rcDatabaseFile);
  }

  RAWDistributor rawDistributor(&wiki, stateFile, recentChangesReader.get());
  bool result = rawDistributor.run(issue, fromPage, singlePage, force, dryRun);
  return result ? 0 : 1;
}
