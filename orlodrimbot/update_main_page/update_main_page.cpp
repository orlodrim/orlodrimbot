// Tool to allow users to edit parts of the main page as if it had a semi-extended cascade protection.
// This works by copying editable versions of templates that contain parts of the main page to protected versions that
// are actually displayed on the main page, while checking that there were no recent edits on templates used recursively
// in those parts.
#include <memory>
#include <string>
#include <utility>
#include "cbl/args_parser.h"
#include "cbl/string.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"
#include "copy_page.h"

using std::make_unique;
using std::pair;
using std::string;
using std::unique_ptr;

int main(int argc, char** argv) {
  string stateFile;
  bool fromRC = false;
  string rcDatabaseFile;
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_SYSOP);
  cbl::parseArgs(argc, argv, &wikiFlags, "--statefile,required", &stateFile, "--fromrc", &fromRC, "--rcdatabasefile",
                 &rcDatabaseFile);
  mwc::Wiki wiki;
  mwc::initWikiFromFlags(wikiFlags, wiki);
  unique_ptr<live_replication::RecentChangesReader> recentChangesReader;
  if (fromRC) {
    recentChangesReader = make_unique<live_replication::RecentChangesReader>(rcDatabaseFile);
  }
  constexpr pair<const char*, const char*> PAGES_TO_COPY[] = {
      {"Modèle:Accueil actualité", "Modèle:Accueil actualité/Copie protégée"},
  };
  string errors;
  for (const auto& [sourcePage, targetPage] : PAGES_TO_COPY) {
    try {
      copyPageIfTemplatesAreUnchanged(wiki, recentChangesReader.get(), stateFile, sourcePage, targetPage);
    } catch (const CopyError& error) {
      cbl::append(errors, "* Erreur lors de la copie de [[", sourcePage, "]] vers [[", targetPage,
                  "]] : ", error.what(), "\n");
    }
  }
  if (!errors.empty()) {
    wiki.writePage("Utilisateur:OrlodrimBot/Statut page d'accueil", errors,
                   mwc::WriteToken::newWithoutConflictDetection(), "Rapport d'erreur");
  }
  return 0;
}
