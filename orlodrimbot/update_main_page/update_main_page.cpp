// Tool to allow users to edit parts of the main page as if it had a semi-extended cascade protection.
// This works by copying editable versions of templates that contain parts of the main page to protected versions that
// are actually displayed on the main page, while checking that there were no recent edits on templates used recursively
// in those parts.
#include <memory>
#include <string>
#include "cbl/args_parser.h"
#include "cbl/error.h"
#include "cbl/file.h"
#include "cbl/json.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"
#include "template_expansion_cache.h"
#include "update_main_page_lib.h"

using std::make_unique;
using std::string;
using std::unique_ptr;

int main(int argc, char** argv) {
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
  string stateFile;
  bool fromRC = false;
  string rcDatabaseFile;
  string expansionCacheFile = ":memory:";
  cbl::parseArgs(argc, argv, &wikiFlags, "--statefile", &stateFile, "--fromrc", &fromRC, "--rcdatabasefile",
                 &rcDatabaseFile, "--expansioncachefile", &expansionCacheFile);

  mwc::Wiki wiki;
  mwc::initWikiFromFlags(wikiFlags, wiki);
  unique_ptr<live_replication::RecentChangesReader> recentChangesReader;
  if (fromRC) {
    recentChangesReader = make_unique<live_replication::RecentChangesReader>(rcDatabaseFile);
  }
  string initialStateJson;
  json::Value state;
  if (!stateFile.empty() && cbl::fileExists(stateFile)) {
    initialStateJson = cbl::readFile(stateFile);
    state = json::parse(initialStateJson);
  }
  cbl::RunOnDestroy saveState([&] {
    if (!stateFile.empty()) {
      string finalStateJson = state.toJSON(json::INDENTED) + "\n";
      if (finalStateJson != initialStateJson) {
        cbl::writeFileAtomically(stateFile, finalStateJson);
      }
    }
  });

  TemplateExpansionCache templateExpansionCache(&wiki, expansionCacheFile);
  updateMainPage(wiki, state, *recentChangesReader, templateExpansionCache);
  return 0;
}
