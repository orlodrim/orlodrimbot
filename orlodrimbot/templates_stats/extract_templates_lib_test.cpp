#include "extract_templates_lib.h"
#include <cstdio>
#include <string>
#include "cbl/file.h"
#include "cbl/log.h"
#include "cbl/tempfile.h"
#include "mwclient/mock_wiki.h"

using cbl::TempFile;
using std::string;

class TemplateExtractorTest {
public:
  void runTests();

private:
  mwc::MockWiki m_wiki;
  TempFile m_templatesFile;
  TempFile m_redirectsFile;
  TempFile m_dumpFile;
  TempFile m_outputFile;
};

void TemplateExtractorTest::runTests() {
  TemplateExtractor templateExtractor(&m_wiki);
  cbl::writeFile(m_templatesFile.path(),
                 "Module:Wikiprojet\n"
                 "Test1\n"
                 "Test2\n"
                 "TemplateWithSubst1\n"
                 "TemplateWithSubst2\n"
                 "RedirectToTest1\n");
  cbl::writeFile(m_redirectsFile.path(),
                 "Modèle:RedirectToTest1|Modèle:Test1\n"
                 "Modèle:OtherRedirectToTest1|Modèle:Test1\n");
  CBL_ASSERT(freopen("testdata/extract_templates_pages_dump.txt", "r", stdin));

  templateExtractor.readTemplates(m_templatesFile.path());
  templateExtractor.readRedirects(m_redirectsFile.path());
  templateExtractor.processDump(m_outputFile.path());

  fclose(stdin);
  string outputFileContent = cbl::readFile(m_outputFile.path());
  CBL_ASSERT_EQ(outputFileContent,
                "Test1|Page 1|{{Test1|{{Test2}}}}\n"
                "Test2|Page 1|{{Test2}}\n"
                "Module:Wikiprojet|Page 1|{{#invoke:Wikiprojet|someFunction|abc}}\n"
                "Module:Wikiprojet|Page 1|{{#invoquE :Wikiprojet|someFunction2|def}}\n"
                "Module:Wikiprojet|Page 1|{{{{{|safesubst:}}}#invoke:Wikiprojet|someFunction3|ghi}}\n"
                "TemplateWithSubst1|Page 1|{{subst:TemplateWithSubst1}}\n"
                "TemplateWithSubst2|Page 1|{{ {{{|safesubst:}}} TemplateWithSubst2}}\n"
                "Test1|Page 2|{{OtherRedirectToTest1|abc}}\n"
                "Test1|Page 2|{{RedirectToTest1|abc}}\n"
                "Test1|Module:Test/Documentation|{{Test1}}\n");
}

int main() {
  TemplateExtractorTest templateExtractorTest;
  templateExtractorTest.runTests();
  return 0;
}
