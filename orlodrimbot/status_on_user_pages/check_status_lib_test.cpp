#include "check_status_lib.h"
#include <string>
#include <unordered_map>
#include <vector>
#include "cbl/log.h"
#include "cbl/unittest.h"
#include "mwclient/mock_wiki.h"
#include "mwclient/wiki.h"

using mwc::UserGroup;
using std::string;
using std::unordered_map;
using std::vector;

class StatusCheckMockWiki : public mwc::MockWiki {
public:
  vector<string> getUsersInGroup(UserGroup userGroup) override { return usersByGroup[userGroup]; }
  vector<string> getCategoryMembers(const string& category) override { return categoryMembers[category]; }
  vector<string> getTransclusions(const string& title) override { return transclusions[title]; }

  unordered_map<UserGroup, vector<string>> usersByGroup;
  unordered_map<string, vector<string>> categoryMembers;
  unordered_map<string, vector<string>> transclusions;
};

class StatusCheckerTest : public cbl::Test {
private:
  CBL_TEST_CASE(update) {
    StatusCheckMockWiki wiki;
    wiki.usersByGroup[mwc::UG_SYSOP] = {"Sysop 1", "Sysop 3"};
    wiki.categoryMembers["Catégorie:Wikipédia:Administrateur Wikipédia"] = {
        "Utilisateur:Sysop 1",
        "Utilisateur:Sysop 1/Présentation",
        "Discussion utilisateur:Sysop 1",
        "Utilisateur:Sysop 2",
        "Utilisateur:Sysop 2/Présentation",
        "Discussion utilisateur:Sysop 2",
        "Catégorie:Wikipédia:Ancien administrateur Wikipédia",
    };
    wiki.transclusions["Modèle:Icône Administrateur"] = {
        "Utilisateur:Sysop 1",         "Utilisateur:Sysop 1/Présentation",
        "Utilisateur:Sysop 2",         "Utilisateur:Sysop 2/Présentation",
        "Modèle:Icône Administrateur",
    };
    updateListOfStatusInconsistencies(wiki, "Test page");
    CBL_ASSERT_EQ(
        wiki.readPageContent("Test page"),
        "<!-- BEGIN BOT SECTION -->\n"
        "* La page [[Utilisateur:Sysop 2]] contient {{m|Icône Administrateur}} "
        "mais {{u'|Sysop 2}} n'est pas membre du groupe « Administrateurs ».\n"
        "* La page [[Utilisateur:Sysop 2]] est dans [[:Catégorie:Wikipédia:Administrateur Wikipédia]] "
        "mais {{u'|Sysop 2}} n'est pas membre du groupe « Administrateurs ».\n"
        "* La page [[Discussion utilisateur:Sysop 2]] est dans [[:Catégorie:Wikipédia:Administrateur Wikipédia]] "
        "mais {{u'|Sysop 2}} n'est pas membre du groupe « Administrateurs ».\n"
        "* La page [[Utilisateur:Sysop 2/Présentation]] est dans [[:Catégorie:Wikipédia:Administrateur Wikipédia]] "
        "mais {{u'|Sysop 2}} n'est pas membre du groupe « Administrateurs ».\n"
        "<!-- END BOT SECTION -->");
  }

  CBL_TEST_CASE(update_emptyList) {
    StatusCheckMockWiki wiki;
    updateListOfStatusInconsistencies(wiki, "Test page");
    CBL_ASSERT_EQ(wiki.readPageContent("Test page"),
                  "<!-- BEGIN BOT SECTION -->\n"
                  "* ''Aucune page détectée''\n"
                  "<!-- END BOT SECTION -->");
  }
};

int main() {
  StatusCheckerTest().run();
  return 0;
}
