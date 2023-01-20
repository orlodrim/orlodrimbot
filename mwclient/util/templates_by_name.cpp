#include "templates_by_name.h"
#include <string_view>
#include "mwclient/parser.h"
#include "mwclient/titles_util.h"
#include "mwclient/wiki.h"

using std::string_view;

namespace wikicode {

TemplatesByNameGenerator::TemplatesByNameGenerator(mwc::Wiki& wiki, Node& node, string_view name,
                                                   EnumerationOrder enumerationOrder)
    : m_generator(&node, enumerationOrder, NT_TEMPLATE), m_titlesUtil(wiki.siteInfo()), m_name(name) {}

bool TemplatesByNameGenerator::next() {
  while (m_generator.next()) {
    Template& template_ = static_cast<Template&>(m_generator.value());
    mwc::TitleParts titleParts = m_titlesUtil.parseTitle(template_.name(), mwc::NS_TEMPLATE);
    if (titleParts.namespaceNumber == mwc::NS_TEMPLATE && titleParts.unprefixedTitle() == m_name) {
      return true;
    }
  }
  return false;
}

}  // namespace wikicode
