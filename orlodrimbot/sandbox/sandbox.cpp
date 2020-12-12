// Resets [[Aide:Bac à sable]] and other sandbox pages.
#include "cbl/args_parser.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/wiki.h"
#include "sandbox_lib.h"

int main(int argc, char** argv) {
  mwc::Wiki wiki;
  bool dryRun = false;
  bool force = false;
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
  cbl::parseArgs(argc, argv, &wikiFlags, "--dryrun", &dryRun, "--force", &force);
  mwc::initWikiFromFlags(wikiFlags, wiki);
  SandboxCleaner sandboxCleaner(&wiki,
                                {
                                    {"Aide:Bac à sable", "Modèle:Préchargement pour Bac à sable"},
                                    {"Discussion aide:Bac à sable", "Modèle:Préchargement pour Discussion Bac à sable"},
                                    // Doing some tests with [[Modèle:Bac à sable]] typically requires including it in
                                    // another page to see the result. Also, it is modified quite infrequently. Thus, it
                                    // is reset only after 30 minutes of inactivity.
                                    {"Modèle:Bac à sable", "Modèle:Préchargement pour modèle Bac à sable", 30 * 60},
                                });
  sandboxCleaner.run(force, dryRun);
  return 0;
}
