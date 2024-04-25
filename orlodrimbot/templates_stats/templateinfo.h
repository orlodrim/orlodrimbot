#ifndef TEMPLATEINFO_H
#define TEMPLATEINFO_H

#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "mwclient/parser.h"
#include "mwclient/wiki.h"
#include "side_template_data.h"

const int MAX_VECTOR_SIZE = 10;

enum OutputFormat {
  OF_TEXT = 1,
  // OF_HTML = 2,
  // OF_HTML_FOR_PHP = 4,
  OF_JSON = 8
};

enum FieldDef {
  FD_NO,
  FD_YES,
  FD_LUA,
};

class FieldInfo {
public:
  FieldInfo() : numArticles(0), numArticlesE(0), numArticlesNE(0), fieldDef(FD_NO), lastTemplateUID(-1) {}
  std::vector<std::string> articles, articlesE, articlesNE, articlesDup;
  int numArticles, numArticlesE, numArticlesNE, numArticlesDup;
  FieldDef fieldDef;
  void addValue(const std::string& title, const std::string& value, int templateUID);

private:
  int lastTemplateUID;
};

class FunctionInfo {
public:
  FunctionInfo() : numArticles(0) {}
  void addCall(const std::string& title);
  std::vector<std::string> articles;
  int numArticles;
};

struct RedirInfo {
  std::vector<std::string> articles;
  int numArticles;
};

struct ArticleErrors {
  std::vector<std::string> badParamsNE;
  std::vector<std::string> badParamsE;
  std::vector<std::string> dupParams;
};

class TemplateInfo {
public:
  enum TemplateNamespace {
    TN_TEMPLATE,
    TN_MODULE,
  };
  TemplateInfo(const std::string& templateName, const std::string& templateCode,
               const SideTemplateData& sideTemplateData);
  void readInclusion(mwc::Wiki& wiki, const std::string& title, const wikicode::Template& template_);
  void generateInfo(FILE* file, OutputFormat format);
  TemplateNamespace namespace_() { return m_namespace; }
  bool hasParameters() { return !m_fieldInfos.empty(); }
  int getArticlesCount() { return m_numArticles; }
  int getErrorsCount() { return m_numErrors; }

private:
  void readTemplateInclusion(mwc::Wiki& wiki, const std::string& title, const wikicode::Template& template_);
  void readModuleInclusion(const std::string& title, const wikicode::Template& template_);
  void extractVars(const std::string& templateCode);
  void generateTextInfo(FILE* file);
  void generateJSONInfo(FILE* file);

  std::string m_templateName;
  std::string m_fullPageName;
  TemplateNamespace m_namespace;
  int m_numInclusions;
  int m_numErrors;
  std::map<std::string, RedirInfo> m_redirInfos;
  std::map<std::string, FieldInfo> m_fieldInfos;
  std::map<std::string, FunctionInfo> m_functionInfos;
  std::vector<std::string> m_articles;
  std::vector<std::string> m_articlesNP;
  int m_numArticles;
  int m_numArticlesNP;
  std::vector<std::pair<std::string, wikicode::List>> m_nestedVariables;
  const SideTemplateData* m_sideTemplateData;
  bool m_inLuaDB;

  std::vector<std::pair<int, std::string>> m_paramByCount;
  std::map<std::string, ArticleErrors> m_articlesWithErrors;
};

#endif
