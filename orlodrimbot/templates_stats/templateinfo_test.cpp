#include "templateinfo.h"
#include <cstdio>
#include <string>
#include "cbl/file.h"
#include "cbl/log.h"
#include "mwclient/mock_wiki.h"
#include "mwclient/parser.h"
#include "side_template_data.h"

using mwc::MockWiki;
using std::string;

int main() {
  MockWiki wiki;
  SideTemplateData sideTemplateData;
  sideTemplateData.loadFromWikicode("<pre>{{Article|auteur=}}</pre>");

  TemplateInfo templateInfo("Article", "{{#invoke:}} {{ {{{|safesubst:}}} #if:1|2|3}}", sideTemplateData);
  wikicode::List list = wikicode::parse("{{RedirectionImaginaire|auteur=abc|def=ghi}}");
  CBL_ASSERT_EQ(list[0].type(), wikicode::NT_TEMPLATE);
  const wikicode::Template& template_ = list[0].asTemplate();
  templateInfo.readInclusion(wiki, "Exemple", template_);
  FILE* outputFile = tmpfile();
  templateInfo.generateInfo(outputFile, OF_TEXT);
  string textOutput = cbl::readOpenedFile(outputFile);
  fclose(outputFile);

  CBL_ASSERT(textOutput.find("\n*pages utilisant la redirection [[Modèle:RedirectionImaginaire]] : 1\n") !=
             string::npos);
  CBL_ASSERT(textOutput.find("\n*auteur (pages : 1, non vide : 1, existe : indirect)") != string::npos);
  CBL_ASSERT(textOutput.find("\n*def (pages : 1, non vide : 1, existe : non)") != string::npos);
  // The use of {{{|safesubst:}}} in the template should not declare "" as a valid parameter.
  CBL_ASSERT(textOutput.find("\n* (pages") == string::npos);
  return 0;
}
