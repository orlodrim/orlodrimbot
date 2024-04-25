#include "side_template_data.h"
#include <re2/re2.h>
#include <string>
#include <vector>
#include "cbl/file.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "cbl/tempfile.h"
#include "cbl/unittest.h"

using std::string;
using std::vector;

class SideTemplateDataTest : public cbl::Test {
private:
  SideTemplateData createHelperFromString(const string& content) {
    cbl::writeFile(m_tempFile.path(), content);
    SideTemplateData sideTemplateData;
    sideTemplateData.loadFromFile(m_tempFile.path());
    return sideTemplateData;
  }

  CBL_TEST_CASE(StandardParameters) {
    SideTemplateData helper = createHelperFromString("<pre>{{TestTemplate|auteur1=x|auteur2=y}}</pre>");
    vector<string> validParams =
        helper.getValidParams("TestTemplate", {{"auteur1", "a"}, {"auteur2", "b"}, {"autre", "c"}});
    CBL_ASSERT_EQ(cbl::join(validParams, ","), "auteur1,auteur2");
  }

  CBL_TEST_CASE(NumberedParameters) {
    SideTemplateData helper =
        createHelperFromString("<pre>{{TestTemplate|a[1-]=|b[5-]=|c[4-50]d=|e3[1-]=|[1-]=|x=}}</pre>");
    vector<string> validParams = helper.getValidParams(
        "TestTemplate", {{"a0", ""},   {"a01", ""}, {"a1", ""},  {"a1z", ""}, {"a999", ""}, {"b4", ""},   {"b5", ""},
                         {"b999", ""}, {"c3d", ""}, {"c4d", ""}, {"c4", ""},  {"c5d", ""},  {"c49d", ""}, {"c50d", ""},
                         {"c51d", ""}, {"e3", ""},  {"e30", ""}, {"e31", ""}, {"e310", ""}, {"e40", ""},  {"0", ""},
                         {"1", ""},    {"999", ""}, {"x", ""},   {"y", ""}});
    CBL_ASSERT_EQ(cbl::join(validParams, ","), "1,999,a1,a999,b5,b999,c49d,c4d,c50d,c5d,e31,e310,x");
  }

  CBL_TEST_CASE(MultinumberedParameters) {
    SideTemplateData helper = createHelperFromString("<pre>{{TestTemplate|a[1-3]b[1-]=}}</pre>");
    // clang-format off
    vector<string> validParams = helper.getValidParams("TestTemplate", {
        {"a0b0", ""}, {"a0b1", ""}, {"a0b99", ""},
        {"a1b0", ""}, {"a1b1", ""}, {"a1b99", ""},
        {"a3b0", ""}, {"a3b1", ""}, {"a3b99", ""},
        {"a4b0", ""}, {"a4b1", ""}, {"a4b99", ""},
        {"a1b01", ""}, {"a1b1z", ""},
    });
    // clang-format on
    CBL_ASSERT_EQ(cbl::join(validParams, ","), "a1b1,a1b99,a3b1,a3b99");
  }

  cbl::TempFile m_tempFile;
};

int main() {
  SideTemplateDataTest().run();
  return 0;
}
