// Archive old sections on talk pages containing {{Archivage par bot}}.
#include <string>
#include <vector>
#include "cbl/args_parser.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/wiki.h"
#include "archiver.h"

using std::string;
using std::vector;

int main(int argc, char** argv) {
  mwc::Wiki wiki;
  string dataDir;
  string keyPrefixFile;
  bool dryRun = false;
  vector<string> pages;
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
  cbl::parseArgs(argc, argv, &wikiFlags, "--datadir", &dataDir, "--keyprefixfile", &keyPrefixFile, "--dryrun", &dryRun,
                 "page", &pages);
  mwc::initWikiFromFlags(wikiFlags, wiki);
  talk_page_archiver::Archiver archiver(&wiki, dataDir, keyPrefixFile, dryRun);
  if (pages.empty()) {
    archiver.archiveAll();
  } else {
    archiver.archivePages(pages);
  }
  return 0;
}
