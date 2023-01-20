#include "archive_template.h"
#include <re2/re2.h>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
#include "cbl/string.h"
#include "mwclient/parser.h"
#include "mwclient/util/templates_by_name.h"
#include "mwclient/wiki.h"
#include "algorithm.h"

using mwc::Wiki;
using std::string;
using std::string_view;
using std::vector;

namespace talk_page_archiver {
namespace {

int parseIntParam(const wikicode::ParsedFields& parsedFields, const string& param, int minValid, int maxValid,
                  int options = 0) {
  const string& valueStr = parsedFields[param];
  if (valueStr.empty()) {
    return ARCHIVE_PARAM_NOT_SET;
  }
  int invalidValue = minValid - 1;
  int value = cbl::parseIntInRange(valueStr, minValid, maxValid, invalidValue, options);
  if (value == invalidValue) {
    throw ParamsInitializationError(cbl::concat("Invalid value for parameter ", param, ": '", valueStr, "'"));
  }
  return value;
}

vector<ParameterizedAlgorithm> parseAlgorithms(const Algorithms& algorithms, string_view algorithmSpecs) {
  static const re2::RE2 reAlgoDescription(R"(([A-Za-z+ ]*)\( *([0-9]+) *d\))");
  vector<ParameterizedAlgorithm> parameterizedAlgorithms;
  for (string_view algorithmSpec : cbl::split(algorithmSpecs, ',', /* ignoreLastFieldIfEmpty = */ true)) {
    string algorithmName;
    int maxAgeInDays = -1;
    const Algorithm* algorithm = nullptr;
    if (RE2::FullMatch(algorithmSpec, reAlgoDescription, &algorithmName, &maxAgeInDays)) {
      try {
        algorithm = &algorithms.find(cbl::toLowerCaseASCII(cbl::trim(algorithmName)));
      } catch (const std::out_of_range&) {
        // algorithm remains null.
      }
    }
    if (algorithm == nullptr) {
      throw ParamsInitializationError(cbl::concat("Invalid algorithm '", algorithmSpec, "'"));
    }
    parameterizedAlgorithms.push_back({.algorithm = algorithm, .maxAgeInDays = maxAgeInDays});
  }
  std::sort(parameterizedAlgorithms.begin(), parameterizedAlgorithms.end(),
            [](const ParameterizedAlgorithm& a1, const ParameterizedAlgorithm& a2) {
              return a1.algorithm->rank() < a2.algorithm->rank();
            });
  return parameterizedAlgorithms;
}

}  // namespace

wikicode::Template* findArchiveTemplate(Wiki& wiki, wikicode::List& parsedCode) {
  for (wikicode::Template& template_ : wikicode::getTemplatesByName(wiki, parsedCode, ARCHIVE_TEMPLATE_NAME)) {
    return &template_;
  }
  return nullptr;
}

bool containsArchiveTemplate(Wiki& wiki, string_view code) {
  wikicode::List parsedCode = wikicode::parse(code);
  return findArchiveTemplate(wiki, parsedCode) != nullptr;
}

ArchiveParams::ArchiveParams(Wiki& wiki, const Algorithms& algorithms, const string& title,
                             wikicode::List& parsedCode) {
  wikicode::Template* archiveTemplate = findArchiveTemplate(wiki, parsedCode);
  if (archiveTemplate == nullptr) {
    throw ParamsInitializationError(cbl::concat("Modèle {{", ARCHIVE_TEMPLATE_NAME, "}} non trouvé"));
  }

  wikicode::ParsedFields parsedFields = archiveTemplate->getParsedFields(wikicode::NORMALIZE_COLLAPSE_VALUE);

  m_rawArchive = parsedFields["archive"];
  m_archive = wiki.normalizeTitle(m_rawArchive);
  if (cbl::startsWith(m_archive, "/")) {
    m_archive = title + m_archive;
  } else if (m_archive.empty()) {
    m_archive = title + "/Archive %(counter)d";
  }

  m_algorithms = parseAlgorithms(algorithms, parsedFields["algo"]);
  if (m_algorithms.empty()) {
    m_algorithms.push_back({.algorithm = &algorithms.find("old"), .maxAgeInDays = 15});
  }

  m_counter = parseIntParam(parsedFields, "counter", 1, 1000000);
  m_minthreadsleft = parseIntParam(parsedFields, "minthreadsleft", 0, 1000000, cbl::MAX_IF_TOO_LARGE);
  m_minthreadstoarchive = parseIntParam(parsedFields, "minthreadstoarchive", 0, 1000000, cbl::MAX_IF_TOO_LARGE);

  const string& maxArchiveSizeStr = parsedFields["maxarchivesize"];
  if (maxArchiveSizeStr.empty()) {
    m_maxarchivesize = ARCHIVE_PARAM_NOT_SET;
  } else {
    static const re2::RE2 reSize(R"((\d+) *[Kk])");
    bool parseResult = RE2::FullMatch(maxArchiveSizeStr, reSize, &m_maxarchivesize);
    if (!parseResult) {
      throw ParamsInitializationError(
          cbl::concat("Invalid value for parameter maxarchivesize: '", maxArchiveSizeStr, "'"));
    }
    // The maximum size of a wiki page is 2 MB.
    m_maxarchivesize = std::min(m_maxarchivesize, 1950);
  }

  m_archiveheader = parsedFields["archiveheader"];
  if (m_archiveheader.empty()) {
    const bool directSubPage =
        cbl::startsWith(m_archive, title + "/") && m_archive.find('/', title.size() + 1) == string::npos;
    if (directSubPage) {
      m_archiveheader = "{{Archive de discussion}}";
    } else {
      m_archiveheader = cbl::concat("{{Archive de discussion|Discussion=", title, "}}");
    }
  }

  m_key = parsedFields["key"];
}

}  // namespace talk_page_archiver
