#ifndef MWC_SITE_INFO_H
#define MWC_SITE_INFO_H

#include <map>
#include <string>
#include <utility>
#include <vector>
#include "cbl/json.h"

namespace mwc {

enum NamespaceNumber {
  NS_MAIN = 0,
  NS_TALK = 1,
  NS_USER = 2,
  NS_USER_TALK = 3,
  NS_PROJECT = 4,
  NS_PROJECT_TALK = 5,
  NS_FILE = 6,
  NS_FILE_TALK = 7,
  NS_MEDIAWIKI = 8,
  NS_MEDIAWIKI_TALK = 9,
  NS_TEMPLATE = 10,
  NS_TEMPLATE_TALK = 11,
  NS_HELP = 12,
  NS_HELP_TALK = 13,
  NS_CATEGORY = 14,
  NS_CATEGORY_TALK = 15,

  NS_SPECIAL = -1,
};

constexpr int SPLIT_TITLE_INTERWIKI = -99;

enum CaseMode {
  CM_CASE_SENSITIVE = 0,
  CM_FIRST_LETTER = 1,
};

class SiteInfo {
public:
  struct Namespace {
    std::string name;  // Does not end with ':'.
    CaseMode caseMode = CM_CASE_SENSITIVE;
  };
  struct InterwikiSpec {
    std::string unnormalizedPrefix;  // Not required to be in lower case.
    std::string language;
  };

  SiteInfo();
  ~SiteInfo();

  // Returns a JSON representation that can parsed with fromJSON. This is not the same as the API representation.
  json::Value toJSONValue() const;
  // Initializes the object from a JSON value previously created with toJSON.
  // Throws: cbl::ParseError.
  void fromJSONValue(const json::Value& value);
  // Initializes the object from a JSON value return by the MediaWiki API (meta=siteinfo).
  // Throws: cbl::ParseError.
  void fromAPIResponse(const json::Value& value);

  const std::map<int, Namespace>& namespaces() const { return m_namespaces; }
  const std::vector<std::pair<std::string, int>>& namespacesByName() const { return m_namespacesByName; }
  const std::map<std::string, int>& aliases() const { return m_aliases; }
  const std::map<std::string, InterwikiSpec>& interwikis() const { return m_interwikis; }
  const Namespace& mainNamespace() const { return *m_mainNamespace; }
  const std::vector<std::string> redirectAliases() const { return m_redirectAliases; }

  static const SiteInfo& stubInstance();

private:
  void initNamespacesByName();

  // Namespace number => namespace.
  std::map<int, Namespace> m_namespaces;
  // Pairs of (lower case name, number) sorted by first member.
  std::vector<std::pair<std::string, int>> m_namespacesByName;
  // Lower case alias => namespace number.
  std::map<std::string, int> m_aliases;
  // Lower case interwiki prefix => interwiki.
  std::map<std::string, InterwikiSpec> m_interwikis;
  const Namespace* m_mainNamespace;
  // Lower case aliases of #REDIRECT (including "#redirect").
  std::vector<std::string> m_redirectAliases;
};

bool isTalkNamespace(int namespace_);

}  // namespace mwc

#endif
