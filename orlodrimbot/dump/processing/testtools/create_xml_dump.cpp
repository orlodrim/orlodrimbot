// Converts ../testdata/frwiki-20000101-pages-meta-current1.txt to an XML file similar to public Wikipedia dumps.
// Many fields that are not used by the dump parser are omitted.
#include <cstdio>
#include <iostream>
#include <string>
#include "cbl/html_entities.h"

using std::string;

void generatePage(const string& title, const string& code, int pageId) {
  int revId = 2000 + pageId;
  int parentRevId = 1000 + pageId;
  string escapedTitle = cbl::escapeHtml(title);
  string escapedCode = cbl::escapeHtml(code);
  printf(R"(  <page>
    <title>%s</title>
    <id>%i</id>
    <revision>
      <id>%i</id>
      <parentid>%i</parentid>
      <timestamp>2000-01-01T00:00:00Z</timestamp>
      <text bytes="123" xml:space="preserve">%s</text>
      <sha1>%isha1</sha1>
    </revision>
  </page>
)",
         escapedTitle.c_str(), pageId, revId, parentRevId, escapedCode.c_str(), revId);
}

int main() {
  string line;
  string title;
  string code;
  int pageId = 0;
  bool afterTitle = false;
  printf("<mediawiki>\n");
  while (getline(std::cin, line)) {
    if (line.size() > 16 && line.starts_with("========") && line.ends_with("========")) {
      if (!title.empty()) {
        generatePage(title, code, ++pageId);
      }
      title = line.substr(8, line.size() - 16);
      code.clear();
      afterTitle = true;
    } else {
      if (!afterTitle) {
        code += '\n';
      }
      code += line;
      afterTitle = false;
    }
  }
  if (!title.empty()) {
    generatePage(title, code, ++pageId);
  }
  printf("</mediawiki>\n");
  return 0;
}
