#include "side_template_data.h"
#include <re2/re2.h>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "cbl/file.h"
#include "cbl/string.h"
#include "cbl/unicode_fr.h"
#include "mwclient/parser.h"
#include "regexp_of_range.h"

using std::map;
using std::string;
using std::string_view;
using std::vector;

string_view stringViewFromStringPiece(re2::StringPiece s) {
  return string_view(s.data(), s.size());
}

void SideTemplateData::loadFromWikicode(const string& code) {
  m_templates.clear();
  string codeNoTags = cbl::replace(code, "<pre>", "");
  cbl::replaceInPlace(codeNoTags, "</pre>", "");
  wikicode::List parsedCode = wikicode::parse(codeNoTags, wikicode::STRICT);
  static const re2::RE2 reNumberedParam(R"(([^\[]*)\[(0|[1-9]\d*)-(0?|[1-9]\d*)\])");
  // re2::StringPiece prefix, suffix;
  // std::string minStr, maxStr;
  for (const wikicode::Template& template_ : parsedCode.getTemplates()) {
    string templateName = unicode_fr::capitalize(template_.name());
    if (templateName.empty() || templateName == "Nobots") continue;
    int iParam = 0;
    TemplateSpec& templateSpec = m_templates[templateName];
    vector<string> regExps;
    string param;
    for (int i = 1; i < template_.size(); i++) {
      template_.splitParamValue(i, &param, nullptr);
      if (param == wikicode::UNNAMED_PARAM) {
        iParam++;
        param = std::to_string(iParam);
      }
      re2::StringPiece paramSuffix = param;
      re2::StringPiece middleText, minStr, maxStr;
      string regExp;
      while (RE2::Consume(&paramSuffix, reNumberedParam, &middleText, &minStr, &maxStr)) {
        if (!maxStr.empty() && (minStr.size() > maxStr.size() || (minStr.size() == maxStr.size() && minStr > maxStr))) {
          regExp.clear();
          break;
        }
        regExp += RE2::QuoteMeta(middleText);
        regExp += '(';
        regExp += buildRegExpForRange(stringViewFromStringPiece(minStr), stringViewFromStringPiece(maxStr));
        regExp += ')';
      }
      if (regExp.empty()) {
        templateSpec.standardParams.insert(param);
      } else {
        regExp += RE2::QuoteMeta(paramSuffix);
        regExps.push_back(std::move(regExp));
      }
    }
    if (!regExps.empty()) {
      templateSpec.regExp = std::make_unique<RE2>(cbl::join(regExps, "|"));
    }
  }
}

void SideTemplateData::loadFromFile(const string& fileName) {
  loadFromWikicode(cbl::readFile(fileName));
}

bool SideTemplateData::isTemplateInLuaDB(const string& templateName) const {
  return m_templates.count(templateName) != 0;
}

vector<string> SideTemplateData::getValidParams(const string& templateName, const map<string, string>& fields) const {
  vector<string> validParams;
  TemplateSpecMap::const_iterator templateSpecIt = m_templates.find(templateName);
  if (templateSpecIt != m_templates.end()) {
    const TemplateSpec& templateSpec = templateSpecIt->second;
    for (const auto& [param, unusedValue] : fields) {
      bool paramIsValid = false;
      if (templateSpec.standardParams.count(param) != 0) {
        paramIsValid = true;
      } else if (templateSpec.regExp) {
        paramIsValid = RE2::FullMatch(param, *templateSpec.regExp);
      }
      if (paramIsValid) {
        validParams.push_back(param);
      }
    }
  }
  return validParams;
}
