// For each page move happening on the wiki, check if the talk page of the moved page had subpages
// (like "/À faire" or "/Admissibilité") and move them too.

#include <algorithm>
#include <string>
#include "cbl/args_parser.h"
#include "cbl/date.h"
#include "cbl/error.h"
#include "cbl/file.h"
#include "cbl/log.h"
#include "cbl/path.h"
#include "cbl/string.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/wiki.h"
#include "move_subpages_lib.h"

using cbl::Date;
using cbl::DateDiff;
using cbl::readFile;
using mwc::Wiki;
using std::string;

Date loadLastDate(const string& stateFileName) {
  Date dateMax = Date::now();
  Date dateMin = dateMax - DateDiff::fromHours(50);
  Date lastDate = dateMin;
  try {
    lastDate = Date::fromISO8601(cbl::trim(readFile(stateFileName)));
  } catch (const cbl::FileNotFoundError&) {
    // This is expected for the first run.
  } catch (const cbl::ParseError&) {
    CBL_ERROR << "Failed to parse the date read from '" << stateFileName << "'";
  }
  return std::min(std::max(lastDate, dateMin), dateMax);
}

void saveLastDate(const string& stateFileName, const Date& date) {
  cbl::writeFile(stateFileName, date.toISO8601() + "\n");
}

int main(int argc, char** argv) {
  Wiki wiki;
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
  string dataDir;
  bool dryRun = false;
  cbl::parseArgs(argc, argv, &wikiFlags, "--datadir", &dataDir, "--dryrun", &dryRun);
  mwc::initWikiFromFlags(wikiFlags, wiki);

  string stateFileName = cbl::joinPaths(dataDir, "last_date.txt");
  Date lastDate = loadLastDate(stateFileName);
  SubpagesMover subpagesMover(&wiki, lastDate, dryRun);
  subpagesMover.processAllMoves();
  if (!dryRun) {
    saveLastDate(stateFileName, subpagesMover.dateOfLastProcessedMove());
  }
  return 0;
}
