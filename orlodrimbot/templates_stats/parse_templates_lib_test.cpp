#include "parse_templates_lib.h"
#include <sstream>
#include <string>
#include "cbl/file.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "cbl/tempfile.h"
#include "mwclient/mock_wiki.h"

using cbl::TempFile;
using std::string;

void checkReadDumpCall(const string& templatesSimpleDump, const string& expectedParam, const string& expectedParamNames,
                       const string& expectedTemplateData) {
  TempFile withParamFile;
  TempFile withParamNamesFile;
  TempFile templateDataFile;

  mwc::MockWiki wiki;
  std::istringstream inputStream(templatesSimpleDump);
  parseTemplatesFromDump(wiki, inputStream, withParamFile.path(), withParamNamesFile.path(), templateDataFile.path());

  CBL_ASSERT_EQ(cbl::readFile(withParamFile.path()), expectedParam);
  CBL_ASSERT_EQ(cbl::readFile(withParamNamesFile.path()), expectedParamNames);
  CBL_ASSERT_EQ(cbl::readFile(templateDataFile.path()), expectedTemplateData);
}

void testReadDump() {
  checkReadDumpCall(
      /* templatesSimpleDump = */
      cbl::unindent(R"(
        Modèle:TemplateStd
         {{x|y={{{variable<noinclude>2</noinclude>}}}|z={{{ {{{z}}} |...}}} }}
        Modèle:TemplateStd/Documentation
         <templatedata>{"params": {"variable1": {}}}</templatedata>
        Modèle:TemplateLua
         {{#invoke:Module1|fonction|x={{{stdvar|}}}}}
        Modèle:TemplateLua/Documentation
         <templatedata>{"params": {"variable1": {"aliases": ["var1", "v1"]}, "variable2": {}}}</templatedata>
        Modèle:SimpleTemplate
         Simple content
        Modèle:Données/A/évolution population
         Simple content
        Modèle:Données/A/informations générales
         Simple content
        Modèle:Redirection vers TemplateStd
         #redirect[[Modèle:TemplateStd]]
      )"),
      /* expectedParam = */
      "TemplateStd|{{{variable}}}{{{ {{{z}}} }}}{{{z}}}\n"
      "TemplateLua|{{{stdvar}}}{{#invoke:A}}\n"
      "SimpleTemplate|\n",
      /* expectedParamNames = */
      "TemplateStd\n"
      "TemplateLua\n"
      "SimpleTemplate\n",
      /* expectedTemplateData = */
      "TemplateLua|{{TemplateLua|v1=|var1=|variable1=|variable2=}}\n");
}

int main() {
  testReadDump();
  return 0;
}
