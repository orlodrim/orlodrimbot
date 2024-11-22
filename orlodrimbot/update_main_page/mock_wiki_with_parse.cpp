#include "mock_wiki_with_parse.h"
#include <re2/re2.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/mock_wiki.h"
#include "mwclient/wiki.h"

using mwc::MockWiki;
using std::string;
using std::string_view;
using std::unordered_map;

static unordered_map<string, string> parseQueryString(string_view query) {
  unordered_map<string, string> parameters;
  for (string_view field : cbl::split(query, '&')) {
    size_t equalPosition = field.find('=');
    parameters[string(field.substr(0, equalPosition))] = cbl::decodeURIComponent(field.substr(equalPosition + 1));
  }
  return parameters;
}

void MockWikiWithParse::resetDatabase() {
  MockWiki::resetDatabase();
  expandTemplatesCallCount = 0;
}

json::Value MockWikiWithParse::apiRequest(const string& request, const string& data, bool canRetry) {
  unordered_map<string, string> parameters = parseQueryString(data);
  CBL_ASSERT_EQ(parameters["action"], "parse");
  CBL_ASSERT_EQ(parameters["prop"], "templates");

  // Simulates the result of the custom parse request by extracting templates directly mentioned in the wikicode.
  static const re2::RE2 reTemplate(R"(\{\{([^{}|]+)[|}])");
  re2::StringPiece toParse = parameters["text"];
  string templateName;
  json::Value result;
  json::Value& templates = result.getMutable("parse").getMutable("templates");
  templates.setToEmptyArray();
  while (RE2::FindAndConsume(&toParse, reTemplate, &templateName)) {
    templates.addItem().getMutable("*") = normalizeTitle(templateName, mwc::NS_TEMPLATE);
  }
  return result;
}

string MockWikiWithParse::expandTemplates(const string& code, const string& title, mwc::revid_t revid) {
  expandTemplatesCallCount++;
  string newCode = code;
  cbl::replaceInPlace(newCode, "{{PAGENAME}}", title);
  cbl::replaceInPlace(newCode, "{{REVISIONID}}", std::to_string(revid));
  cbl::replaceInPlace(newCode, "{{", "{{expanded:");
  return newCode;
}
