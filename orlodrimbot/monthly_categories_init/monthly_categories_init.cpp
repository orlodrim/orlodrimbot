// At the beginning of the month, create hidden categories that will be filled with pages where a specific maintenance
// template is added during that month, such as "Catégorie:Admissibilité à vérifier depuis septembre 2020"
// (filled by {{Admissibilité à vérifier|date=septembre 2020}}).
#include <string>
#include <string_view>
#include "cbl/args_parser.h"
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/wiki.h"

using cbl::Date;
using cbl::DateDiff;
using std::string;
using std::string_view;

struct CategoryConfig {
  string_view titlePattern;
  string_view templateName;
};

constexpr CategoryConfig CATEGORY_CONFIGS[] = {
    {"Catégorie:Admissibilité à vérifier depuis %(monthname)s %(year)d",
     "Utilisateur:OrlodrimBot/Préchargement/Admissibilité à vérifier"},
    {"Catégorie:Article manquant de références depuis %(monthname)s %(year)d",
     "Utilisateur:OrlodrimBot/Préchargement/Article manquant de référence"},
    {"Catégorie:Article orphelin depuis %(monthname)s %(year)d",
     "Utilisateur:OrlodrimBot/Préchargement/Article orphelin"},
    {"Catégorie:Article à wikifier depuis %(monthname)s %(year)d",
     "Utilisateur:OrlodrimBot/Préchargement/Article à wikifier"},
};

constexpr string_view FRENCH_MONTHS[12] = {"janvier", "février", "mars",      "avril",   "mai",      "juin",
                                           "juillet", "août",    "septembre", "octobre", "novembre", "décembre"};

void initCategory(mwc::Wiki& wiki, const CategoryConfig& config, const Date& dateForInitialization, bool dryRun) {
  int month = dateForInitialization.month();
  CBL_ASSERT(month >= 1 && month <= 12) << dateForInitialization;
  int year = dateForInitialization.year();
  CBL_ASSERT(year >= 1000 && year <= 9999) << dateForInitialization;
  string month02 = (month < 10 ? "0" : "") + std::to_string(month);

  string title = cbl::replace(config.titlePattern, "%(monthname)s", FRENCH_MONTHS[month - 1]);
  cbl::replaceInPlace(title, "%(year)d", std::to_string(year));
  CBL_ASSERT(title.find("%(") == string::npos) << "Invalid category titlePattern: '" << config.titlePattern << "'";
  string content =
      "{{subst:" + string(config.templateName) + "|mois=" + month02 + "|année=" + std::to_string(year) + "}}";

  if (wiki.pageExists(title)) {
    CBL_INFO << "The page '" << title << "' already exists";
    return;
  }

  CBL_INFO << "Writing '" << title << "' with content '" << content << "'";
  if (!dryRun) {
    // The edit summary is intentionally left blank so that the autosummary shows the content before substitution.
    wiki.writePage(title, content, mwc::WriteToken::newForCreation(), "");
  }
}

int main(int argc, char** argv) {
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
  // This program normally runs in the evening, a few hours before midnight UTC. Set dateForInitialization to six
  // hours in the future so that categories for a given month are created at the end of the last day of the previous
  // month.
  Date dateForInitialization = Date::now() + DateDiff(3600 * 6);
  bool dryRun = false;
  cbl::parseArgs(argc, argv, &wikiFlags, "--date", &dateForInitialization, "--dryrun", &dryRun);
  mwc::Wiki wiki;
  mwc::initWikiFromFlags(wikiFlags, wiki);

  if (dateForInitialization.day() == 1) {
    CBL_INFO << "Monthly initialization: yes, day == 1";
    for (const CategoryConfig& config : CATEGORY_CONFIGS) {
      CBL_INFO << "Processing pattern '" << config.titlePattern << "'";
      try {
        initCategory(wiki, config, dateForInitialization, dryRun);
      } catch (const mwc::WikiError& error) {
        CBL_ERROR << error.what();
      }
    }
  } else {
    CBL_INFO << "Monthly initialization: no, day != 1";
  }
  return 0;
}
