#include "mwclient/parser_misc.h"
#include <string>
#include "cbl/log.h"
#include "cbl/unittest.h"

using std::string;

namespace wikicode {

class ParserMiscTest : public cbl::Test {
private:
  CBL_TEST_CASE(isSpaceOrComment) {
    CBL_ASSERT(isSpaceOrComment(""));
    CBL_ASSERT(isSpaceOrComment(" "));
    CBL_ASSERT(isSpaceOrComment(" \t\r\n"));
    CBL_ASSERT(isSpaceOrComment("<!---->"));
    CBL_ASSERT(isSpaceOrComment("<!-- test"));
    CBL_ASSERT(isSpaceOrComment("<!-- test -->"));
    CBL_ASSERT(isSpaceOrComment(" <!-- comment -->\n<!-- some other comment --> "));
    CBL_ASSERT(!isSpaceOrComment("a"));
    CBL_ASSERT(!isSpaceOrComment(" a "));
    CBL_ASSERT(!isSpaceOrComment(" <!-- test -->."));
    CBL_ASSERT(!isSpaceOrComment(string(1, '\0')));
    CBL_ASSERT(isSpaceOrComment("<!--" + string(1, '\0') + "-->"));
    CBL_ASSERT(!isSpaceOrComment("<!--" + string(1, '\0') + "-->a"));
  }

  CBL_TEST_CASE(stripComments) {
    CBL_ASSERT_EQ(stripComments(""), "");
    CBL_ASSERT_EQ(stripComments("test"), "test");
    CBL_ASSERT_EQ(stripComments("<!-- test -->"), "");
    CBL_ASSERT_EQ(stripComments("a<!-- test -->c"), "ac");
    CBL_ASSERT_EQ(stripComments("<!-- test"), "");
    CBL_ASSERT_EQ(stripComments("a<!-- test"), "a");
    CBL_ASSERT_EQ(stripComments("a<!--test-->b<!--test2-->c"), "abc");
    CBL_ASSERT_EQ(stripComments("a<!---->b-->c"), "ab-->c");
    CBL_ASSERT_EQ(stripComments("a<!--->b-->c"), "ac");
    string nul = string(1, '\0');
    CBL_ASSERT_EQ(stripComments("a" + nul + "b<!--" + nul + "-->c"), "a" + nul + "bc");
  }

  CBL_TEST_CASE(stripCommentsInPlace) {
    string s;
    s = "test";
    stripCommentsInPlace(s);
    CBL_ASSERT_EQ(s, "test");
    s = "anoth<!-- Comment 1 -->er tes<!-- Comment 2 -->t";
    stripCommentsInPlace(s);
    CBL_ASSERT_EQ(s, "another test");
  }

  CBL_TEST_CASE(escape) {
    CBL_ASSERT_EQ(escape(""), "<nowiki></nowiki>");
    CBL_ASSERT_EQ(escape("abc"), "<nowiki>abc</nowiki>");
    CBL_ASSERT_EQ(escape("[[test]]"), "<nowiki>[[test]]</nowiki>");
    CBL_ASSERT_EQ(escape("RFC 1234"), "<nowiki>RFC 1234</nowiki>");
    CBL_ASSERT_EQ(escape("http://www.example.com/"), "<nowiki>http://www.example.com/</nowiki>");
    CBL_ASSERT_EQ(escape("[//www.example.com]"), "<nowiki>[//www.example.com]</nowiki>");
    CBL_ASSERT_EQ(escape("''test''"), "<nowiki>''test''</nowiki>");
    CBL_ASSERT_EQ(escape("</nowiki>"), "<nowiki>&lt;/nowiki></nowiki>");
    CBL_ASSERT_EQ(escape("&amp;"), "<nowiki>&amp;amp;</nowiki>");
  }

  CBL_TEST_CASE(getTitleLevel) {
    // Standard cases.
    CBL_ASSERT_EQ(getTitleLevel("Content"), 0);
    CBL_ASSERT_EQ(getTitleLevel("=Content="), 1);
    CBL_ASSERT_EQ(getTitleLevel("==Content=="), 2);
    CBL_ASSERT_EQ(getTitleLevel("===Content==="), 3);
    // Extra spaces are ignored.
    CBL_ASSERT_EQ(getTitleLevel("== Content=="), 2);
    CBL_ASSERT_EQ(getTitleLevel("== Content =="), 2);
    CBL_ASSERT_EQ(getTitleLevel("==Content== "), 2);
    CBL_ASSERT_EQ(getTitleLevel("==  Content   ==    "), 2);
    // Extra spaces at the beginning are not ignored.
    CBL_ASSERT_EQ(getTitleLevel(" ==Content=="), 0);
    // Unbalanced number of '='.
    CBL_ASSERT_EQ(getTitleLevel("==Content"), 0);
    CBL_ASSERT_EQ(getTitleLevel("==Content="), 1);
    CBL_ASSERT_EQ(getTitleLevel("Content=="), 0);
    CBL_ASSERT_EQ(getTitleLevel("=Content=="), 1);
    // Special cases.
    CBL_ASSERT_EQ(getTitleLevel(""), 0);
    CBL_ASSERT_EQ(getTitleLevel("="), 0);
    CBL_ASSERT_EQ(getTitleLevel("=="), 0);
    CBL_ASSERT_EQ(getTitleLevel("= ="), 1);
    CBL_ASSERT_EQ(getTitleLevel("==="), 1);
    CBL_ASSERT_EQ(getTitleLevel("===="), 1);
    CBL_ASSERT_EQ(getTitleLevel("====="), 2);
  }

  CBL_TEST_CASE(getTitleContent) {
    // Standard cases.
    CBL_ASSERT_EQ(getTitleContent("=Title 1="), "Title 1");
    CBL_ASSERT_EQ(getTitleContent("==Title 2=="), "Title 2");
    CBL_ASSERT_EQ(getTitleContent("===Title 3==="), "Title 3");
    // Extra spaces are ignored.
    CBL_ASSERT_EQ(getTitleContent("== Title 4=="), "Title 4");
    CBL_ASSERT_EQ(getTitleContent("== Title 5 =="), "Title 5");
    CBL_ASSERT_EQ(getTitleContent("==Title 6== "), "Title 6");
    CBL_ASSERT_EQ(getTitleContent("==  Title 7   ==    "), "Title 7");
    // Unbalanced number of '='.
    CBL_ASSERT_EQ(getTitleContent("==Title 8="), "=Title 8");
    CBL_ASSERT_EQ(getTitleContent("=Title 9=="), "Title 9=");
    CBL_ASSERT_EQ(getTitleContent("= Title 10 =="), "Title 10 =");
    // Special cases.
    CBL_ASSERT_EQ(getTitleContent("= ="), "");
    CBL_ASSERT_EQ(getTitleContent("==="), "=");
    CBL_ASSERT_EQ(getTitleContent("===="), "==");
    CBL_ASSERT_EQ(getTitleContent("====="), "=");
  }
};

}  // namespace wikicode

int main() {
  wikicode::ParserMiscTest().run();
  return 0;
}
