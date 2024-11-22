// Defines algorithms that are specific to the French Wikipedia, e.g. because they depend on a template from this wiki.
// See https://fr.wikipedia.org/wiki/Mod%C3%A8le:Archivage_par_bot for a description of all supported algorithms.
#include "frwiki_algorithms.h"
#include <re2/re2.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/parser.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/wikiutil/date_parser.h"
#include "algorithm.h"
#include "thread_util.h"

using std::make_unique;
using std::string;
using std::string_view;
using std::unordered_set;
using wikiutil::DateParser;
using wikiutil::SignatureDate;

namespace talk_page_archiver {

class EraseNewslettersAlgorithm : public Algorithm {
public:
  EraseNewslettersAlgorithm() : Algorithm("erasenewsletters") {}
  RunResult run(const mwc::Wiki& wiki, string_view threadContent) const override {
    static const unordered_set<string> NEWSLETTERS_LISTS = {
        "Global message delivery/Targets/GLAM",
        "Global message delivery/Targets/Signpost",
        "Global message delivery/Targets/Tech ambassadors",
        "Global message delivery/Targets/This Month in Education",
        "Global message delivery/Targets/Wikidata",
        "Global message delivery/Targets/Wikimedia Highlights",
        "User:Johan (WMF)/Tech News target list 3",
        "VisualEditor/Newsletter",
    };
    bool isNewsletter = false;
    for (string_view line : cbl::splitLines(threadContent)) {
      if (line.empty()) continue;
      isNewsletter = false;
      if (line.starts_with("<!-- Message envoyé par ")) {
        size_t titleParam = line.find("title=");
        if (titleParam == string_view::npos) continue;
        size_t titleValueStart = titleParam + strlen("title=");
        size_t titleValueEnd = line.find_first_of("& ", titleValueStart);
        if (titleValueEnd == string_view::npos) continue;
        string title = cbl::decodeURIComponent(line.substr(titleValueStart, titleValueEnd - titleValueStart));
        cbl::replaceInPlace(title, "_", " ");
        if (NEWSLETTERS_LISTS.count(title) != 0) {
          isNewsletter = true;
        } else {
          CBL_INFO << "Unknown massmessage list: " << title;
        }
      } else if (line.starts_with("{{RAW/PdD|") || line.starts_with("{{Wikimag message|")) {
        isNewsletter = true;
      }
    }
    return {.action = isNewsletter ? ThreadAction::ERASE : ThreadAction::KEEP};
  }
};

class FdNAlgorithm : public Algorithm {
public:
  FdNAlgorithm() : Algorithm("fdn") {}
  RunResult run(const mwc::Wiki& wiki, string_view threadContent) const override {
    static const unordered_set<string> FDN_TEMPLATES = {
        "Modèle:Répondu",
        "Modèle:Publication",
        "Modèle:Forum des nouveaux hors-sujet",
        "Modèle:FdNHS",
        "Modèle:FDNHS",
        "Modèle:Forum des nouveaux brouillon",
        "Modèle:FdNBrouillon",
        "Modèle:Forum des nouveaux déjà publié",
        "Modèle:FdNDP",
        "Modèle:Forum des nouveaux copyvio",
        "Modèle:CopyvioFdN",
        "Modèle:FdNadm",
        "Modèle:Réponse wikicode",
    };
    static const unordered_set<string> NON_FINAL_STATES = {
        "non",
        // "en attente",
        // "attente",
        "autre avis",
        "autre",
        "en cours",
        "encours",
    };
    wikicode::List parsedContent = wikicode::parse(threadContent);
    bool templateFound = false;
    for (const wikicode::Template& template_ : parsedContent.getTemplates()) {
      string templateName = wiki.normalizeTitle(template_.name(), mwc::NS_TEMPLATE);
      if (templateName == "Modèle:Réponse wikicode" || templateName == "Modèle:Réponse FdN") {
        string valueOfParam1 = template_.getParsedFields()["1"];
        if (!valueOfParam1.empty() && NON_FINAL_STATES.count(valueOfParam1) == 0) {
          templateFound = true;
          break;
        }
      } else if (FDN_TEMPLATES.count(templateName) != 0) {
        templateFound = true;
        break;
      }
    }
    return {.action = templateFound ? ThreadAction::ARCHIVE : ThreadAction::KEEP};
  }
};

class CheckInTitleAlgorithm : public Algorithm {
public:
  CheckInTitleAlgorithm() : Algorithm("checked+old") {}
  RunResult run(const mwc::Wiki& wiki, string_view threadContent) const override {
    static const re2::RE2 reChecked(
        R"(\{\{\s*([Ff]ait|[Nn]on|[Oo]ui|[Dd]éplacée|[Ss]uppression +immédiate|[Hh][Cc]|[Cc]roix3|[Pp]as +fait|)"
        R"([Aa]F)\s*[|}])");
    bool match = RE2::PartialMatch(wikicode::stripComments(extractThreadTitle(threadContent)), reChecked);
    return {.action = match ? ThreadAction::ARCHIVE : ThreadAction::KEEP};
  }
};

class OldTitleAlgorithm : public Algorithm {
public:
  OldTitleAlgorithm() : Algorithm("oldtitle") {}
  RunResult run(const mwc::Wiki& wiki, string_view threadContent) const override {
    SignatureDate dateInTitle = {.utcDate = computeDateInTitle(threadContent, /* maxForMissingFields = */ true)};
    SignatureDate dateInContent = DateParser::getByLang("fr").extractMaxSignatureDate(threadContent);
    if (dateInTitle.isNull()) {
      return {.action = ThreadAction::KEEP};
    } else {
      return {.action = ThreadAction::ARCHIVE, .forcedDate = {.utcDate = std::max(dateInTitle, dateInContent)}};
    }
  }
};

Algorithms getFrwikiAlgorithms() {
  Algorithms algorithms;
  // The order is important. For instance, if the bot was just enabled on a page with "erasenewsletters(1d),old(2d)",
  // sections that match both algorithms (i.e. newsletters older than 2 days) should be erased and not archived.
  algorithms.add(make_unique<EraseNewslettersAlgorithm>());
  algorithms.add(make_unique<FdNAlgorithm>());
  algorithms.add(make_unique<CheckInTitleAlgorithm>());
  algorithms.add(make_unique<OldTitleAlgorithm>());
  algorithms.add(make_unique<ArchiveOldSectionsAlgorithm>());
  algorithms.add(make_unique<EraseOldSectionsAlgorithm>());
  return algorithms;
}

}  // namespace talk_page_archiver
