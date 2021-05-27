#include "check_status_lib.h"
#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/titles_util.h"
#include "mwclient/util/bot_section.h"
#include "mwclient/wiki.h"

using mwc::TitleParts;
using mwc::UserGroup;
using mwc::Wiki;
using std::pair;
using std::string;
using std::string_view;
using std::unordered_set;
using std::vector;

namespace {

const char* getFrenchNameOfGroup(UserGroup group) {
  switch (group) {
    case mwc::UG_SYSOP:
      return "Administrateurs";
    case mwc::UG_BOT:
      return "Robots";
    case mwc::UG_BUREAUCRAT:
      return "Bureaucrates";
    case mwc::UG_CHECKUSER:
      return "Vérificateurs d’utilisateurs";
    case mwc::UG_OVERSIGHT:
      return "Masqueurs de modifications";
    case mwc::UG_INTERFACE_ADMIN:
      return "Administrateurs d’interface";
    default:
      return "";
  }
}

struct Inconsistency {
  string page;         // Page in the user or user talk namespace with an inconsistency.
  string user;         // Derived from `page`.
  UserGroup group;     // Group mentioned in the user page but that the user is not part of.
  string pageElement;  // Free-text description of the part of the page that indicates that `user` is in `group`.

  string describe() const {
    return "* La page [[" + page + "]] " + pageElement + " mais {{u'|" + user + "}} n'est pas membre du groupe « " +
           getFrenchNameOfGroup(group) + " ».\n";
  }
};

string getUserFromPage(Wiki& wiki, const string& page) {
  TitleParts titleParts = wiki.parseTitle(page);
  if (titleParts.namespaceNumber == mwc::NS_USER || titleParts.namespaceNumber == mwc::NS_USER_TALK) {
    string_view userName = titleParts.unprefixedTitle();
    size_t slashPosition = userName.find('/');
    if (slashPosition != string_view::npos) {
      userName = cbl::trim(userName.substr(0, slashPosition));
    }
    return string(userName);
  } else {
    return string();
  }
}

void enumInconsistenciesForGroup(Wiki& wiki, UserGroup group, const vector<string>& categories,
                                 const vector<string>& templates, vector<Inconsistency>& inconsistencies) {
  CBL_INFO << "Reading members of group '" << getFrenchNameOfGroup(group) << "'";
  vector<string> usersInGroupVec = wiki.getUsersInGroup(group);
  unordered_set<string> usersInGroup(usersInGroupVec.begin(), usersInGroupVec.end());
  auto processPages = [&](const vector<string>& pages, bool checkSubpages, const string& pageElement) {
    for (const string& page : pages) {
      if (page.find('/') != string::npos && !checkSubpages) {
        continue;
      }
      string user = getUserFromPage(wiki, page);
      if (!user.empty() && usersInGroup.count(user) == 0) {
        inconsistencies.emplace_back();
        Inconsistency& inconsistency = inconsistencies.back();
        inconsistency.page = page;
        inconsistency.user = user;
        inconsistency.group = group;
        inconsistency.pageElement = pageElement;
      }
    }
  };
  for (const string& category : categories) {
    CBL_INFO << "Reading members of category '" << category << "'";
    processPages(wiki.getCategoryMembers(category), true, "est dans [[:" + category + "]]");
  }
  for (const string& template_ : templates) {
    CBL_INFO << "Reading transclusions of template '" << template_ << "'";
    string shortTemplateName = wiki.stripNamespace(template_, mwc::NS_TEMPLATE);
    processPages(wiki.getTransclusions(template_), false, "contient {{m|" + shortTemplateName + "}}");
  }
}

pair<string, int> getTitleSortKey(Wiki& wiki, const string& title) {
  mwc::TitleParts titleParts = wiki.parseTitle(title);
  return {string(titleParts.unprefixedTitle()), titleParts.namespaceNumber};
}

vector<Inconsistency> enumInconsistencies(Wiki& wiki) {
  vector<Inconsistency> inconsistencies;
  enumInconsistenciesForGroup(wiki, mwc::UG_SYSOP, {"Catégorie:Administrateur Wikipédia"},
                              {"Modèle:Icône Administrateur", "Modèle:Icône Opérateur"}, inconsistencies);
  enumInconsistenciesForGroup(wiki, mwc::UG_BUREAUCRAT, {"Catégorie:Bureaucrate Wikipédia"}, {}, inconsistencies);
  enumInconsistenciesForGroup(wiki, mwc::UG_CHECKUSER, {"Catégorie:Wikipédia:Vérificateur d'adresses IP"}, {},
                              inconsistencies);
  enumInconsistenciesForGroup(wiki, mwc::UG_OVERSIGHT, {"Catégorie:Masqueur Wikipédia"}, {}, inconsistencies);
  enumInconsistenciesForGroup(
      wiki, mwc::UG_INTERFACE_ADMIN, {},
      {"Modèle:Utilisateur Wikipédia:Administrateur d'interface", "Modèle:Icône Administrateur d'interface"},
      inconsistencies);
  std::sort(inconsistencies.begin(), inconsistencies.end(),
            [&](const Inconsistency& inconsistency1, const Inconsistency& inconsistency2) {
              if (inconsistency1.page != inconsistency2.page) {
                return getTitleSortKey(wiki, inconsistency1.page) < getTitleSortKey(wiki, inconsistency2.page);
              } else if (inconsistency1.group != inconsistency2.group) {
                return inconsistency1.group < inconsistency2.group;
              } else {
                return inconsistency1.pageElement < inconsistency2.pageElement;
              }
            });
  return inconsistencies;
}

}  // namespace

void updateListOfStatusInconsistencies(Wiki& wiki, const string& listPage) {
  vector<Inconsistency> inconsistencies = enumInconsistencies(wiki);
  string botSection;
  for (const Inconsistency& inconsistency : inconsistencies) {
    botSection += inconsistency.describe();
  }
  if (botSection.empty()) {
    botSection = "* ''Aucune page détectée''\n";
  }
  mwc::WriteToken writeToken;
  string code = wiki.readPageContentIfExists(listPage, &writeToken);
  mwc::replaceBotSection(code, botSection);
  wiki.writePage(listPage, code, writeToken, "Mise à jour");
}
