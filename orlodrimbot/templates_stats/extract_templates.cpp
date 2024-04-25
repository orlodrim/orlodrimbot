#include <string>
#include "cbl/args_parser.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/wiki.h"
#include "extract_templates_lib.h"

using std::string;

int main(int argc, char** argv) {
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
  string redirects;                // File with templates redirects (required).
  string templatesWithParameters;  // File containing the list of templates with parameters (required).
  string outputFileName;           // Output file with all inclusions.
  cbl::parseArgs(argc, argv, &wikiFlags, "--redirects,required", &redirects, "--templates-names,required",
                 &templatesWithParameters, "--output", &outputFileName);
  mwc::Wiki wiki;
  mwc::initWikiFromFlags(wikiFlags, wiki);

  TemplateExtractor templateExtractor(&wiki);
  templateExtractor.readTemplates(templatesWithParameters);
  templateExtractor.readRedirects(redirects);
  templateExtractor.processDump(outputFileName);
  return 0;
}
