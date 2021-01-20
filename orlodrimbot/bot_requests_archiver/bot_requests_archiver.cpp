// Archive requests on https://fr.wikipedia.org/wiki/Wikipédia:Bot/Requêtes
#include "cbl/args_parser.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/wiki.h"
#include "bot_requests_archiver_lib.h"

int main(int argc, char** argv) {
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
  bool dryRun = false;
  bool forceNewMonth = false;
  cbl::parseArgs(argc, argv, &wikiFlags, "--dryrun", &dryRun, "--forcenewmonth", &forceNewMonth);
  mwc::Wiki wiki;
  mwc::initWikiFromFlags(wikiFlags, wiki);
  BotRequestsArchiver archiver(wiki, dryRun);
  archiver.run(forceNewMonth);
  return 0;
}
