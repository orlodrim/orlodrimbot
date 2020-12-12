#ifndef MWC_TITLES_UTIL_H
#define MWC_TITLES_UTIL_H

#include <string>
#include <string_view>
#include "site_info.h"

namespace mwc {

struct TitleParts {
  std::string title;
  int unprefixedTitleBegin = 0;
  int anchorBegin = 0;
  int namespaceNumber = NS_MAIN;

  std::string_view namespace_() const { return std::string_view(title).substr(0, unprefixedTitleBegin); }
  std::string_view unprefixedTitle() const {
    return std::string_view(title).substr(unprefixedTitleBegin, anchorBegin - unprefixedTitleBegin);
  }
  std::string_view anchor() const { return std::string_view(title).substr(anchorBegin); }
  std::string_view titleWithoutAnchor() const { return std::string_view(title).substr(0, anchorBegin); }
  void clearAnchor() { title.resize(anchorBegin); }
};

enum ParseTitleFlags {
  PTF_DECODE_URI_COMPONENT = 1,
  PTF_NAMESPACE_ONLY = 2,
  PTF_KEEP_INITIAL_COLON = 4,

  PTF_DEFAULT = 0,
  PTF_LINK_TARGET = PTF_DECODE_URI_COMPONENT,
};

class TitlesUtil {
public:
  explicit TitlesUtil(const SiteInfo& siteInfo) : m_siteInfo(&siteInfo) {}

  TitleParts parseTitle(std::string_view title, NamespaceNumber defaultNamespaceNumber = NS_MAIN,
                        int parseTitleFlags = PTF_DEFAULT) const;
  int getTitleNamespace(std::string_view title) const;

  std::string getTalkPage(const std::string& title) const;
  std::string getSubjectPage(const std::string& title) const;

  // Returns a link to target by adding double square brackets around the target.
  // In some particular cases such as categories and files, also adds a colon before the target to force MediaWiki to
  // interpret the syntax as a standard link.
  std::string makeLink(const std::string& target) const;

private:
  std::string getSubjectOrTalkPageHelper(const std::string& title, int lowerBit) const;

  const SiteInfo* m_siteInfo;
};

}  // namespace mwc

#endif
