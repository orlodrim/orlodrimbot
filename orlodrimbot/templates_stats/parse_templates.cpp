// Extract templates parameters from a dump of templates.
// Input flags:
//   --templatesdump: A "simple dump" of all templates on the wiki (see processSimpleDump in parse_templates_lib.cpp for
//     the format).
// Output flags:
//   --withparam: A filtered list of templates in the dump and their parameters (uninteresting templates such as
//     documentation pages without parameters are excluded).
//     Format:
//       Template1|{{{param1}}}{{{param2}}}
//       Template2|{{{param3}}}
//     This is only a pre-parsing step to generate a smaller file (in earlier versions, all the code of templates was
//     kept there).
//     In particular, this may still contain nested variables such as "{{{ someprefix{{{param}}} }}}" and duplicate
//     parameters. The parsing is done again in templateinfo.cpp.
//   --withparamnames: Only the names of templates written to --withparam (one per line).
//   --templatedata: Parameters extracted from the <templatedata> of templates or their documentation page.
//     For instance, if "Modèle:Exemple/Documentation" contains:
//       <templatedata>{"params":{"p1":{"aliases":["q1"]},"p2":{}}}</templatedata>
//     The output file will contain:
//       Exemple|{{Exemple|p1=|q1=|p2=}}
//     This matches the format used in Utilisateur:Orlodrim/LuaConfig which was the original way of specifying
//     parameters for templates using modules.
#include <fstream>
#include <string>
#include "cbl/args_parser.h"
#include "cbl/log.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/wiki.h"
#include "parse_templates_lib.h"

using std::ifstream;
using std::string;

int main(int argc, char** argv) {
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
  string templatesDumpPath;
  string withParam;
  string withParamNames;
  string templateData;
  cbl::parseArgs(argc, argv, &wikiFlags, "--templatesdump,required", &templatesDumpPath, "--withparam,required",
                 &withParam, "--withparamnames,required", &withParamNames, "--templatedata,required", &templateData);

  mwc::Wiki wiki;
  mwc::initWikiFromFlags(wikiFlags, wiki);

  ifstream templatesDumpStream(templatesDumpPath.c_str());
  CBL_ASSERT(templatesDumpStream) << "Cannot read from '" << templatesDumpPath << "'";
  parseTemplatesFromDump(wiki, templatesDumpStream, withParam, withParamNames, templateData);

  return 0;
}
