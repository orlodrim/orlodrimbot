#include "parser_misc.h"
#include <ctype.h>
#include <cstring>
#include <string>
#include <string_view>
#include "cbl/string.h"

using std::string;
using std::string_view;

namespace wikicode {

bool isSpaceOrComment(string_view s) {
  while (!s.empty()) {
    if (isspace(static_cast<unsigned char>(s[0]))) {
      s.remove_prefix(1);
    } else if (s.starts_with("<!--")) {
      size_t commentEnd = s.find("-->", 4);
      if (commentEnd == string_view::npos) break;
      s.remove_prefix(commentEnd + 3);
    } else {
      return false;
    }
  }
  return true;
}

string stripComments(string_view s) {
  string result;
  while (!s.empty()) {
    size_t commentStart = s.find("<!--");
    if (commentStart == string_view::npos) break;
    result += s.substr(0, commentStart);
    size_t commentEnd = s.find("-->", commentStart + 4);
    s.remove_prefix(commentEnd != string_view::npos ? commentEnd + 3 : s.size());
  }
  result += s;
  return result;
}

void stripCommentsInPlace(string& s) {
  if (s.find("<!--") != string::npos) {
    s = stripComments(s);
  }
}

string escape(string_view code) {
  string escapedCode = cbl::replace(code, "&", "&amp;");
  cbl::replaceInPlace(escapedCode, "<", "&lt;");
  // Predicting if MediaWiki could interpret anything is hard, so we add <nowiki> unconditionally.
  // For instance, raw URLs and magic strings such as "RFC 1234" are displayed as external links.
  return cbl::concat("<nowiki>", escapedCode, "</nowiki>");
}

static string_view stripEqualSigns(string_view line) {
  string_view titleContent = cbl::trim(line, cbl::TRIM_RIGHT);
  while (titleContent.size() > 2 && titleContent.front() == '=' && titleContent.back() == '=') {
    titleContent.remove_prefix(1);
    titleContent.remove_suffix(1);
  }
  return titleContent;
}

int getTitleLevel(string_view line) {
  return stripEqualSigns(line).data() - line.data();
}

string getTitleContent(string_view line) {
  return string(cbl::trim(stripEqualSigns(line)));
}

}  // namespace wikicode
