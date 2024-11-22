#include "wiki.h"
#include <ctype.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "cbl/http_client.h"
#include "cbl/unicode_fr.h"
#include "site_info.h"
#include "titles_util.h"
#include "wiki_defs.h"

using cbl::HTTPClient;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::vector;

namespace mwc {

static void skipSpace(string_view& code) {
  for (; !code.empty() && isspace(static_cast<unsigned char>(code[0])); code.remove_prefix(1)) {}
}

static bool parseChar(string_view& code, char c) {
  if (!code.empty() && code[0] == c) {
    code.remove_prefix(1);
    return true;
  } else {
    return false;
  }
}

Wiki::Wiki() {
  setHTTPClient(std::make_unique<HTTPClient>());
  m_httpClient->setUserAgent("Orlodrim mwclient library");
}

Wiki::~Wiki() {}

// == HTTP ==

HTTPClient& Wiki::httpClient() {
  return *m_httpClient;
}

void Wiki::setHTTPClient(unique_ptr<cbl::HTTPClient> httpClient) {
  if (!httpClient) {
    throw std::invalid_argument("Wiki::setHTTPClient called with null httpClient");
  } else if (!m_wikiURL.empty()) {
    throw InvalidStateError("Wiki::setHTTPClient cannot be called after Wiki::login");
  }
  m_httpClient = std::move(httpClient);
}

void Wiki::setDelayBeforeRequests(int delay) {
  m_httpClient->setDelayBeforeRequests(delay);
  m_delayBeforeRequestsOverridden = true;
}

void Wiki::setDelayBetweenEdits(int delay) {
  m_delayBetweenEdits = delay;
  m_delayBetweenEditsOverridden = true;
}

// == Parsing (titles and redirects) ==

TitleParts Wiki::parseTitle(string_view title, NamespaceNumber defaultNamespaceNumber, int parseTitleFlags) const {
  return TitlesUtil(m_siteInfo).parseTitle(title, defaultNamespaceNumber, parseTitleFlags);
}

string Wiki::normalizeTitle(string_view title, NamespaceNumber defaultNamespaceNumber) const {
  return TitlesUtil(m_siteInfo).parseTitle(title, defaultNamespaceNumber).title;
}

string Wiki::stripNamespace(string_view title, NamespaceNumber expectedNamespace) const {
  TitleParts titleParts = parseTitle(title);
  return titleParts.namespaceNumber == expectedNamespace ? string(titleParts.unprefixedTitle()) : string();
}

int Wiki::getTitleNamespace(string_view title) const {
  return TitlesUtil(m_siteInfo).getTitleNamespace(title);
}

string Wiki::getTalkPage(const string& title) const {
  return TitlesUtil(m_siteInfo).getTalkPage(title);
}

string Wiki::getSubjectPage(const string& title) const {
  return TitlesUtil(m_siteInfo).getSubjectPage(title);
}

string Wiki::makeLink(const string& target) const {
  return TitlesUtil(m_siteInfo).makeLink(target);
}

bool Wiki::readRedirect(string_view code, string* target, string* anchor) const {
  skipSpace(code);
  if (code.empty() || code[0] != '#') {
    return false;
  }
  size_t redirectKeywordEnd = code.find_first_of(" :[\n");
  if (redirectKeywordEnd == string_view::npos) {
    return false;
  }
  string redirectWord = unicode_fr::toLowerCase(code.substr(0, redirectKeywordEnd));
  const vector<string>& redirectAliases = m_siteInfo.redirectAliases();
  if (std::find(redirectAliases.begin(), redirectAliases.end(), redirectWord) == redirectAliases.end()) {
    return false;
  }
  code.remove_prefix(redirectKeywordEnd);
  skipSpace(code);
  if (parseChar(code, ':')) {
    skipSpace(code);
  }
  if (!(parseChar(code, '[') && parseChar(code, '['))) {
    return false;
  }
  size_t linkEnd = code.find_first_of("]|\n");
  if (linkEnd == string_view::npos || code[linkEnd] == '\n') {
    return false;
  }
  if (target || anchor) {
    TitleParts titleParts = parseTitle(code.substr(0, linkEnd), NS_MAIN, PTF_LINK_TARGET);
    if (anchor) {
      *anchor = titleParts.anchor();
    }
    if (target) {
      titleParts.clearAnchor();
      *target = std::move(titleParts.title);
    }
  }
  return true;
}

}  // namespace mwc
