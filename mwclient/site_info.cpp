#include "site_info.h"
#include <algorithm>
#include <string>
#include <utility>
#include "cbl/error.h"
#include "cbl/json.h"
#include "cbl/unicode_fr.h"

using std::pair;
using std::string;

namespace mwc {

const SiteInfo::Namespace DEFAULT_NAMESPACE;

SiteInfo::SiteInfo() : m_mainNamespace(&DEFAULT_NAMESPACE) {}

SiteInfo::~SiteInfo() {}

json::Value SiteInfo::toJSONValue() const {
  json::Value result;
  result.getMutable("siteinfo_version") = 1;
  json::Value& namespaces = result.getMutable("namespaces");
  namespaces.setToEmptyObject();
  for (const auto& [namespaceNumber, namespace_] : m_namespaces) {
    json::Value& namespaceObj = namespaces.getMutable(namespace_.name);
    namespaceObj.getMutable("number") = namespaceNumber;
    namespaceObj.getMutable("casemode") = namespace_.caseMode;
  }
  json::Value& aliases = result.getMutable("aliases");
  aliases.setToEmptyObject();
  for (const auto& [alias, namespaceNumber] : m_aliases) {
    aliases.getMutable(alias) = namespaceNumber;
  }
  json::Value& interwikis = result.getMutable("interwikis");
  interwikis.setToEmptyObject();
  for (const pair<const string, InterwikiSpec>& keyAndInterwikiSpec : m_interwikis) {
    const InterwikiSpec& interwikiSpec = keyAndInterwikiSpec.second;
    json::Value& interwikiObj = interwikis.getMutable(interwikiSpec.unnormalizedPrefix);
    interwikiObj.setToEmptyObject();
    if (!interwikiSpec.language.empty()) {
      interwikiObj.getMutable("lang") = interwikiSpec.language;
    }
  }
  json::Value& redirectAliases = result.getMutable("redirect-aliases");
  redirectAliases.setToEmptyArray();
  for (const string& redirectAlias : m_redirectAliases) {
    redirectAliases.addItem() = redirectAlias;
  }
  return result;
}

void SiteInfo::fromJSONValue(const json::Value& value) {
  if (value["siteinfo_version"].numberAsInt() != 1) {
    throw cbl::ParseError("Invalid value passed to SiteInfo::fromJSONValue");
  }

  m_namespaces.clear();
  m_aliases.clear();
  m_interwikis.clear();
  m_redirectAliases.clear();

  for (const auto& [namespaceName, namespaceObj] : value["namespaces"]) {
    int namespaceNumber = namespaceObj["number"].numberAsInt();
    Namespace& namespace_ = m_namespaces[namespaceNumber];
    namespace_.name = namespaceName;
    namespace_.caseMode = static_cast<CaseMode>(namespaceObj["casemode"].numberAsInt());
  }
  if (m_namespaces.count(NS_MAIN) == 0) {
    throw cbl::ParseError("Invalid value passed to SiteInfo::fromJSONValue (missing main namespace)");
  }
  for (const auto& [alias, namespaceNumber] : value["aliases"]) {
    m_aliases[alias] = namespaceNumber.numberAsInt();
  }
  for (const auto& [interwikiName, interwikiObj] : value["interwikis"]) {
    InterwikiSpec& interwikiSpec = m_interwikis[unicode_fr::toLowerCase(interwikiName)];
    interwikiSpec.unnormalizedPrefix = interwikiName;
    interwikiSpec.language = interwikiObj["lang"].str();
  }
  for (const json::Value& alias : value["redirect-aliases"].array()) {
    m_redirectAliases.push_back(alias.str());
  }

  initNamespacesByName();
}

bool convertStringToCaseMode(const string& caseModeStr, CaseMode& caseMode) {
  if (caseModeStr == "case-sensitive") {
    caseMode = CM_CASE_SENSITIVE;
  } else if (caseModeStr == "first-letter") {
    caseMode = CM_FIRST_LETTER;
  } else {
    return false;
  }
  return true;
}

void SiteInfo::fromAPIResponse(const json::Value& value) {
  const json::Value& namespacesNode = value["namespaces"];
  const json::Value& aliasesNode = value["namespacealiases"];
  const json::Value& iwmapNode = value["interwikimap"];
  const json::Value& magicWords = value["magicwords"];

  if (!namespacesNode.isObject() || !aliasesNode.isArray() || !iwmapNode.isArray() || !magicWords.isArray()) {
    throw cbl::ParseError("missing element in 'query' node");
  }

  m_namespaces.clear();
  m_aliases.clear();
  m_interwikis.clear();
  m_redirectAliases.clear();

  for (const pair<const string, json::Value>& namespaceIt : namespacesNode) {
    const json::Value& nsNode = namespaceIt.second;
    int id = nsNode["id"].numberAsInt();
    Namespace& namespace_ = m_namespaces[id];
    namespace_.name = nsNode["*"].str();
    const string& caseModeStr = nsNode["case"].str();
    if (!convertStringToCaseMode(caseModeStr, namespace_.caseMode)) {
      throw cbl::ParseError("cannot parse case mode '" + caseModeStr + "'");
    }
    m_aliases[unicode_fr::toLowerCase(namespace_.name)] = id;
    if (nsNode.has("canonical")) {
      const string& canonicalName = nsNode["canonical"].str();
      m_aliases[unicode_fr::toLowerCase(canonicalName)] = id;
    }
  }
  if (m_namespaces.count(NS_MAIN) == 0) {
    throw cbl::ParseError("no main namespace");
  }

  for (const json::Value& aliasNode : aliasesNode.array()) {
    const string& name = aliasNode["*"].str();
    m_aliases[unicode_fr::toLowerCase(name)] = aliasNode["id"].numberAsInt();
  }

  for (const json::Value& iwNode : iwmapNode.array()) {
    const string& unnormalizedPrefix = iwNode["prefix"].str();
    InterwikiSpec& interwikiSpec = m_interwikis[unicode_fr::toLowerCase(unnormalizedPrefix)];
    interwikiSpec.unnormalizedPrefix = unnormalizedPrefix;
    interwikiSpec.language = iwNode["language"].str();
  }

  for (const json::Value& magicWord : magicWords.array()) {
    if (magicWord["name"].str() == "redirect") {
      for (const json::Value& alias : magicWord["aliases"].array()) {
        m_redirectAliases.push_back(unicode_fr::toLowerCase(alias.str()));
      }
    }
  }

  initNamespacesByName();
}

void SiteInfo::initNamespacesByName() {
  m_namespacesByName.clear();
  for (const auto& [namespaceNumber, namespace_] : m_namespaces) {
    m_namespacesByName.emplace_back(namespace_.name, namespaceNumber);
  }
  std::sort(m_namespacesByName.begin(), m_namespacesByName.end());
  m_mainNamespace = &m_namespaces.at(NS_MAIN);
}

const SiteInfo& SiteInfo::stubInstance() {
  static const SiteInfo emptySiteInfo;
  return emptySiteInfo;
}

bool isTalkNamespace(int namespace_) {
  return (namespace_ & 1) != 0;
}

}  // namespace mwc
