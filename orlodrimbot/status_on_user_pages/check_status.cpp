// Generates a list of user pages with a category, user box or icon that wrongly indicates that the user has a specific
// status, like sysop or bureaucrat.
#include "cbl/args_parser.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/wiki.h"
#include "check_status_lib.h"

int main(int argc, char** argv) {
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
  cbl::parseArgs(argc, argv, &wikiFlags);
  mwc::Wiki wiki;
  mwc::initWikiFromFlags(wikiFlags, wiki);
  updateListOfStatusInconsistencies(wiki, "Utilisateur:OrlodrimBot/Indications de statut");
  return 0;
}
