#ifndef MWC_UTIL_TEMPLATES_BY_NAME_H
#define MWC_UTIL_TEMPLATES_BY_NAME_H

#include <string>
#include <string_view>
#include "cbl/generated_range.h"
#include "mwclient/parser.h"
#include "mwclient/titles_util.h"
#include "mwclient/wiki.h"

namespace wikicode {

class TemplatesByNameGenerator {
public:
  TemplatesByNameGenerator(mwc::Wiki& wiki, Node& node, std::string_view name, EnumerationOrder enumerationOrder);
  bool next();

protected:
  NodeGenerator m_generator;
  mwc::TitlesUtil m_titlesUtil;
  std::string m_name;
};

class TemplatesByNameNonConstGenerator : public TemplatesByNameGenerator {
public:
  using value_type = Template&;
  using TemplatesByNameGenerator::TemplatesByNameGenerator;
  Template& value() const { return static_cast<Template&>(m_generator.value()); }
};

class TemplatesByNameConstGenerator : public TemplatesByNameGenerator {
public:
  using value_type = const Template&;
  using TemplatesByNameGenerator::TemplatesByNameGenerator;
  const Template& value() const { return static_cast<Template&>(m_generator.value()); }
};

inline cbl::GeneratedRange<TemplatesByNameNonConstGenerator> getTemplatesByName(
    mwc::Wiki& wiki, Node& node, std::string_view name, EnumerationOrder enumerationOrder = wikicode::PREFIX_DFS) {
  return cbl::GeneratedRange<TemplatesByNameNonConstGenerator>(wiki, node, name, enumerationOrder);
}

inline cbl::GeneratedRange<TemplatesByNameConstGenerator> getTemplatesByName(
    mwc::Wiki& wiki, const Node& node, std::string_view name,
    EnumerationOrder enumerationOrder = wikicode::PREFIX_DFS) {
  return cbl::GeneratedRange<TemplatesByNameConstGenerator>(wiki, const_cast<Node&>(node), name, enumerationOrder);
}

}  // namespace wikicode

#endif
