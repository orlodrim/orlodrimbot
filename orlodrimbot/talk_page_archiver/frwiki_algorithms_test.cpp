#include "frwiki_algorithms.h"
#include <string_view>
#include "cbl/log.h"
#include "cbl/unittest.h"
#include "mwclient/mock_wiki.h"
#include "algorithm.h"

using mwc::MockWiki;
using std::string_view;

namespace talk_page_archiver {

class FrwikiAlgorithmsTest : public cbl::Test {
private:
  void setUp() override { m_algorithms = getFrwikiAlgorithms(); }
  bool containsFdNTemplate(string_view text) {
    return m_algorithms.find("fdn").run(m_wiki, text).action == ThreadAction::ARCHIVE;
  }
  CBL_TEST_CASE(containsFdNTemplate) {
    MockWiki wiki;
    CBL_ASSERT(!containsFdNTemplate("{{Réponse FdN}}"));
    CBL_ASSERT(!containsFdNTemplate("{{Réponse FdN|autre}}"));
    CBL_ASSERT(containsFdNTemplate("{{Réponse FdN|oui}}"));
    CBL_ASSERT(containsFdNTemplate("{{Réponse FdN|attente}}"));
    CBL_ASSERT(!containsFdNTemplate("{{Réponse FdN|encours}}"));
    CBL_ASSERT(containsFdNTemplate("{{ template : réponse_FdN\n| 1 = oui }}"));
    CBL_ASSERT(containsFdNTemplate("{{Répondu}}"));
    CBL_ASSERT(containsFdNTemplate("{{ répondu }}"));
    CBL_ASSERT(!containsFdNTemplate("<nowiki>{{Réponse FdN|oui}}</nowiki>"));
    CBL_ASSERT(!containsFdNTemplate("<!--{{Réponse FdN|oui}}-->"));
    CBL_ASSERT(containsFdNTemplate("{{Publication}}"));
    CBL_ASSERT(containsFdNTemplate("{{Forum des nouveaux hors-sujet}}"));
    CBL_ASSERT(containsFdNTemplate("{{FdNHS}}"));
    CBL_ASSERT(containsFdNTemplate("{{Forum des nouveaux brouillon}}"));
    CBL_ASSERT(containsFdNTemplate("{{FdNBrouillon}}"));
    CBL_ASSERT(containsFdNTemplate("{{Forum des nouveaux déjà publié}}"));
    CBL_ASSERT(containsFdNTemplate("{{FdNDP}}"));
  }

  Algorithms m_algorithms;
  mwc::MockWiki m_wiki;
};

}  // namespace talk_page_archiver

int main() {
  talk_page_archiver::FrwikiAlgorithmsTest().run();
  return 0;
}
