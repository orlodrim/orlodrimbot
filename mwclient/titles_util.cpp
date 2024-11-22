#include "titles_util.h"
#include <map>
#include <string>
#include <string_view>
#include "cbl/html_entities.h"
#include "cbl/string.h"
#include "cbl/unicode_fr.h"
#include "cbl/utf8.h"
#include "site_info.h"

using std::map;
using std::string;
using std::string_view;

namespace mwc {

static bool getNamespaceFromNormalizedString(const SiteInfo& siteInfo, string_view str, int& namespace_) {
  int searchMin = 0;
  int searchMax = siteInfo.namespacesByName().size();
  while (searchMin < searchMax) {
    int middle = (searchMin + searchMax) >> 1;
    const auto& [namespaceName, namespaceNumber] = siteInfo.namespacesByName()[middle];
    int cmp = str.compare(namespaceName);
    if (cmp > 0) {
      searchMin = middle + 1;
    } else if (cmp < 0) {
      searchMax = middle;
    } else {
      namespace_ = namespaceNumber;
      return true;
    }
  }
  return false;
}

static void appendNormalizedTitlePart(string& output, string_view titlePart, CaseMode caseMode) {
  output.reserve(output.size() + titlePart.size());
  bool firstChar = true;
  bool spaceInBuffer = false;
  while (true) {
    int c = cbl::utf8::consumeChar(titlePart);
    if (c == 0 || c == -1) {
      break;
    } else if (c == ' ' || c == '_' || c == 0xA0 || c == '\n' || c == '\t') {
      spaceInBuffer = !firstChar;
    } else if (c == 0x200E || c == 0x200F) {
      // Ignore left-to-right and right-to-left markers
    } else {
      if (spaceInBuffer) {
        output += ' ';
        spaceInBuffer = false;
      }
      cbl::utf8::EncodeBuffer encodeBuffer;
      if (firstChar && caseMode == CM_FIRST_LETTER) {
        output += unicode_fr::charToTitleCase(c, encodeBuffer);
      } else {
        output += cbl::utf8::encode(c, encodeBuffer);
      }
      firstChar = false;
    }
  }
}

TitleParts TitlesUtil::parseTitle(string_view title, NamespaceNumber defaultNamespaceNumber,
                                  int parseTitleFlags) const {
  string decodingBuffer;
  if ((parseTitleFlags & PTF_DECODE_URI_COMPONENT) && title.find('%') != string_view::npos) {
    decodingBuffer = cbl::decodeURIComponent(title);
    title = decodingBuffer;
  }
  if (title.find('&') != string_view::npos) {
    decodingBuffer = cbl::unescapeHtml(title);
    title = decodingBuffer;
  }

  int namespaceNumber = defaultNamespaceNumber;
  const string* namespaceName = nullptr;
  CaseMode caseMode = CM_CASE_SENSITIVE;
  size_t colonPosition = title.find(':');
  if (colonPosition != string_view::npos) {
    if (colonPosition != 0 &&
        getNamespaceFromNormalizedString(*m_siteInfo, title.substr(0, colonPosition), namespaceNumber)) {
      // This branch is a performance optimization in case the namespace is already normalized.
      const SiteInfo::Namespace& namespace_ = m_siteInfo->namespaces().at(namespaceNumber);
      namespaceName = &namespace_.name;
      caseMode = namespace_.caseMode;
    } else {
      string maybeNamespace;
      appendNormalizedTitlePart(maybeNamespace, title.substr(0, colonPosition), CM_CASE_SENSITIVE);
      // If the title starts with a colon, skip it and reset the namespace to NS_MAIN.
      if (maybeNamespace.empty() && !(parseTitleFlags & PTF_KEEP_INITIAL_COLON)) {
        namespaceNumber = NS_MAIN;
        title.remove_prefix(colonPosition + 1);
        colonPosition = title.find(':');
        if (colonPosition != string::npos) {
          appendNormalizedTitlePart(maybeNamespace, title.substr(0, colonPosition), CM_CASE_SENSITIVE);
        }
      }
      // Check if the content before the first colon is a namespace.
      if (!maybeNamespace.empty()) {
        string maybeNamespaceLowerCase = unicode_fr::toLowerCase(maybeNamespace);
        map<string, int>::const_iterator aliasIt = m_siteInfo->aliases().find(maybeNamespaceLowerCase);
        if (aliasIt != m_siteInfo->aliases().end()) {
          namespaceNumber = aliasIt->second;
          const SiteInfo::Namespace& namespace_ = m_siteInfo->namespaces().at(namespaceNumber);
          namespaceName = &namespace_.name;
          caseMode = namespace_.caseMode;
        } else {
          map<string, SiteInfo::InterwikiSpec>::const_iterator iwIt =
              m_siteInfo->interwikis().find(maybeNamespaceLowerCase);
          if (iwIt != m_siteInfo->interwikis().end()) {
            namespaceNumber = SPLIT_TITLE_INTERWIKI;
            namespaceName = &iwIt->second.unnormalizedPrefix;
          }
        }
      }
    }
  }

  TitleParts titleParts;
  titleParts.namespaceNumber = namespaceNumber;
  if (!(parseTitleFlags & PTF_NAMESPACE_ONLY)) {
    string_view anchor;
    size_t anchorPosition = title.find('#');
    if (anchorPosition != string_view::npos) {
      anchor = title.substr(anchorPosition);
      title = title.substr(0, anchorPosition);
    }
    if (namespaceName == nullptr && namespaceNumber == NS_MAIN) {
      appendNormalizedTitlePart(titleParts.title, title, m_siteInfo->mainNamespace().caseMode);
    } else {
      string_view unnormalizedUnprefixedTitle;
      if (namespaceName == nullptr) {
        const SiteInfo::Namespace& namespace_ = m_siteInfo->namespaces().at(namespaceNumber);
        namespaceName = &namespace_.name;
        caseMode = namespace_.caseMode;
        unnormalizedUnprefixedTitle = title;
      } else {
        unnormalizedUnprefixedTitle = title.substr(colonPosition + 1);
      }
      titleParts.title.reserve(namespaceName->size() + 1 + unnormalizedUnprefixedTitle.size());
      titleParts.title += *namespaceName;
      titleParts.title += ':';
      appendNormalizedTitlePart(titleParts.title, unnormalizedUnprefixedTitle, caseMode);
      titleParts.unprefixedTitleBegin = namespaceName->size() + 1;
    }
    titleParts.anchorBegin = titleParts.title.size();
    appendNormalizedTitlePart(titleParts.title, anchor, CM_CASE_SENSITIVE);
  }
  return titleParts;
}

int TitlesUtil::getTitleNamespace(string_view title) const {
  return parseTitle(title, NS_MAIN, PTF_LINK_TARGET | PTF_NAMESPACE_ONLY).namespaceNumber;
}

string TitlesUtil::getSubjectOrTalkPageHelper(const string& title, int lowerBit) const {
  string newTitle;
  TitleParts titleParts = parseTitle(title, NS_MAIN, PTF_LINK_TARGET);
  int newNamespace = (titleParts.namespaceNumber & ~1) | lowerBit;
  string_view unprefixedTitle = titleParts.unprefixedTitle();
  if (titleParts.namespaceNumber != SPLIT_TITLE_INTERWIKI && !unprefixedTitle.empty()) {
    map<int, SiteInfo::Namespace>::const_iterator namespaceIt = m_siteInfo->namespaces().find(newNamespace);
    if (namespaceIt != m_siteInfo->namespaces().end()) {
      if (newNamespace != NS_MAIN) {
        newTitle.reserve(namespaceIt->second.name.size() + 1 + unprefixedTitle.size());
        newTitle += namespaceIt->second.name;
        newTitle += ":";
      }
      newTitle += unprefixedTitle;
    }
  }
  return newTitle;
}

string TitlesUtil::getTalkPage(const string& title) const {
  return getSubjectOrTalkPageHelper(title, 1);
}

string TitlesUtil::getSubjectPage(const string& title) const {
  string subjectPage = getSubjectOrTalkPageHelper(title, 0);
  if (subjectPage.empty()) {
    subjectPage = title;
  }
  return subjectPage;
}

string TitlesUtil::makeLink(const string& target) const {
  int namespace_ = getTitleNamespace(target);
  bool colonNeeded =
      !target.starts_with(":") && (namespace_ == NS_CATEGORY || namespace_ == NS_FILE || target.starts_with("/"));
  string link;
  link.reserve(target.size() + 4 + (colonNeeded ? 1 : 0));
  link += colonNeeded ? "[[:" : "[[";
  link += target;
  link += "]]";
  return link;
}

}  // namespace mwc
