#include "tweet_proposals.h"
#include "cbl/date.h"
#include "cbl/log.h"
#include "mwclient/mock_wiki.h"

using cbl::Date;

namespace newsletters {
namespace {

const char TWEETS_PAGE[] = "Wikipédia:Réseaux sociaux/Publications";

class TweetProposalsTest {
public:
  void run();

private:
  mwc::MockWiki m_wiki;
};

void TweetProposalsTest::run() {
  Date::setFrozenValueOfNow(Date::fromISO8601("2018-05-17T00:00:00Z"));
  m_wiki.setPageContent(TWEETS_PAGE,
                        "{{Page de discussion}}\n"
                        "== Jeudi 17 mai ==\n"
                        "{{Proposition tweet|texte=#ImageDuJour Bâtiments de la Speicherstadt, à Hambourg, site du "
                        "patrimoine mondial de l'Unesco.}}\n"
                        "== Vendredi 18 mai ==\n"
                        "{{Proposition tweet|texte=#ImageDuJour Gros plan sur un chou romanesco.}}\n"
                        "== Samedi 19 mai ==\n"
                        "== Programmations ultérieures ==");
  TweetProposals tweetProposals(&m_wiki);
  tweetProposals.load();
  tweetProposals.addProposal("{{Proposition tweet|test1}}");
  tweetProposals.addProposalWithDate("{{Proposition tweet|test2}}", Date::fromISO8601("2018-05-19T00:00:00Z"));
  tweetProposals.writePage("test");
  CBL_ASSERT_EQ(m_wiki.readPageContent(TWEETS_PAGE),
                                 "{{Page de discussion}}\n"
                                 "== Jeudi 17 mai ==\n"
                                 "{{Proposition tweet|texte=#ImageDuJour Bâtiments de la Speicherstadt, à Hambourg, "
                                 "site du patrimoine mondial de l'Unesco.}}\n"
                                 "== Vendredi 18 mai ==\n"
                                 "{{Proposition tweet|test1}}\n"
                                 "\n"
                                 "{{Proposition tweet|texte=#ImageDuJour Gros plan sur un chou romanesco.}}\n"
                                 "== Samedi 19 mai ==\n"
                                 "{{Proposition tweet|test2}}\n"
                                 "\n"
                                 "== Programmations ultérieures ==");
}

}  // namespace
}  // namespace newsletters

int main() {
  newsletters::TweetProposalsTest tweetProposalsTest;
  tweetProposalsTest.run();
  return 0;
}
