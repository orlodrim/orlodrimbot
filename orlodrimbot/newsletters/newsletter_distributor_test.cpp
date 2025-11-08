#include "newsletter_distributor.h"
#include <string>
#include <vector>
#include "cbl/log.h"
#include "cbl/unittest.h"
#include "mwclient/mock_wiki.h"

using std::string;
using std::vector;

namespace newsletters {

class NewsletterDistributorTest : public cbl::Test {
private:
  void checkGetSubscribers(const string& code, const string& expectedSubscribersStr) {
    m_wiki.setPageContent("Test list of subscribers", code);
    vector<Subscriber> subscribers = getSubscribers(m_wiki, "Test list of subscribers");
    string actualSubscribersStr;
    for (const Subscriber& subscriber : subscribers) {
      actualSubscribersStr += subscriber.page + (subscriber.deleteOldMessages ? "|deleteOldMessages" : "") + "\n";
    }
    CBL_ASSERT_EQ(actualSubscribersStr, expectedSubscribersStr);
  }
  CBL_TEST_CASE(getSubscribers) {
    checkGetSubscribers(
        "Header\n"
        "* {{#target:User:FirstUser|fr.wikipedia.org}}\n"
        "* {{#target:User:SecondUser|fr.wikipedia.org}} {{Ne pas purger les anciens numéros}}\n"
        "Footer",
        "Discussion utilisateur:FirstUser|deleteOldMessages\n"
        "Discussion utilisateur:SecondUser\n");
    checkGetSubscribers("* {{Abonnement Bistro}}\n", "Wikipédia:Le Bistro\n");
    checkGetSubscribers("* {{#target:User:TestUser}}\n", "Discussion utilisateur:TestUser|deleteOldMessages\n");
    checkGetSubscribers("* {{#target:User:TestUser/Newsletters}}\n",
                        "Discussion utilisateur:TestUser/Newsletters|deleteOldMessages\n");
    checkGetSubscribers("* {{#target:User:TestUser/Newsletters}} {{BeBot nopurge}}\n",
                        "Discussion utilisateur:TestUser/Newsletters\n");
    checkGetSubscribers("*{{Modèle:beBot_nopurge}}{{ #TARGET: UtilisatricE : TestUser }}\n",
                        "Discussion utilisateur:TestUser\n");
    checkGetSubscribers("* {{#target:Discussion Projet:Test}}\n", "Discussion Projet:Test|deleteOldMessages\n");
    checkGetSubscribers("* {{#target:Discussion Portail:Test}}\n", "Discussion Portail:Test|deleteOldMessages\n");
    checkGetSubscribers("* {{#target:Article}}\n", "");
    checkGetSubscribers("# {{#target:User:TestUser}}\n", "Discussion utilisateur:TestUser|deleteOldMessages\n");
  }

  mwc::MockWiki m_wiki;
};

}  // namespace newsletters

int main() {
  newsletters::NewsletterDistributorTest().run();
  return 0;
}
