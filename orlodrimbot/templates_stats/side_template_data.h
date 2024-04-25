#ifndef SIDE_TEMPLATE_DATA_H
#define SIDE_TEMPLATE_DATA_H

#include <re2/re2.h>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class SideTemplateData {
public:
  SideTemplateData() = default;
  SideTemplateData(const SideTemplateData&) = delete;
  SideTemplateData(SideTemplateData&&) = default;
  SideTemplateData& operator=(const SideTemplateData&) = delete;

  void loadFromFile(const std::string& fileName);
  void loadFromWikicode(const std::string& wcode);
  // templateName is the template name without a namespace.
  bool isTemplateInLuaDB(const std::string& templateName) const;
  std::vector<std::string> getValidParams(const std::string& templateName,
                                          const std::map<std::string, std::string>& fields) const;

private:
  struct Range {
    int min;
    int max;
  };
  struct TemplateSpec {
    std::unordered_set<std::string> standardParams;
    std::unique_ptr<re2::RE2> regExp;
  };
  using TemplateSpecMap = std::unordered_map<std::string, TemplateSpec>;
  TemplateSpecMap m_templates;
};

#endif
