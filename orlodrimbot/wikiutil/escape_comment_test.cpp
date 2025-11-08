#include "escape_comment.h"
#include "cbl/log.h"
#include "cbl/unittest.h"
#include "mwclient/mock_wiki.h"

namespace wikiutil {

class EscapeCommentTest : public cbl::Test {
private:
  CBL_TEST_CASE(AllCases) {
    mwc::MockWiki wiki;
    CBL_ASSERT_EQ(escapeComment(wiki, "Test"), "Test");
    CBL_ASSERT_EQ(escapeComment(wiki, "[[Test]]"), "[[Test]]");
    CBL_ASSERT_EQ(escapeComment(wiki, "[[Catégorie:Test]]"), "[[:Catégorie:Test]]");
    CBL_ASSERT_EQ(escapeComment(wiki, "[[Fichier:Test]]"), "[[:Fichier:Test]]");
    CBL_ASSERT_EQ(escapeComment(wiki, "[[:Catégorie:Test]]"), "[[:Catégorie:Test]]");
    CBL_ASSERT_EQ(escapeComment(wiki, "{{Test}}"), "<nowiki>{{Test}}</nowiki>");
    CBL_ASSERT_EQ(escapeComment(wiki, "[[Modèle:Test|{{Test}}]]"), "[[Modèle:Test|<nowiki>{{Test}}</nowiki>]]");
    CBL_ASSERT_EQ(escapeComment(wiki, "https://example.com"), "<nowiki>https://example.com</nowiki>");
  }
};

}  // namespace wikiutil

int main() {
  wikiutil::EscapeCommentTest().run();
  return 0;
}
