#include "templateinfo.h"
#include <re2/re2.h>
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include "cbl/log.h"
#include "cbl/string.h"
#include "cbl/utf8.h"
#include "mwclient/parser.h"
#include "mwclient/wiki.h"
#include "json.h"
#include "side_template_data.h"

using mwc::Wiki;
using std::make_pair;
using std::map;
using std::pair;
using std::string;
using std::unordered_set;
using std::vector;

namespace {

const re2::RE2 reNumericField("[1-9][0-9]*");

bool containsInvoke(const string& code) {
  static const re2::RE2 reModuleInvoke("(?i:#invoke|#invoque)\\s*:");
  return RE2::PartialMatch(code, reModuleInvoke);
}

void pushInVector(vector<string>& v, int& size, const string& value, int maxCount) {
  if (v.empty() || v.back() != value) {
    if ((int) v.size() >= maxCount) {
      v.pop_back();
    }
    v.push_back(value);
    size++;
  }
}

}  // namespace

void FieldInfo::addValue(const string& title, const string& value, int templateUID) {
  if (lastTemplateUID == templateUID) {
    pushInVector(articlesDup, numArticlesDup, title, 5000);
  }
  lastTemplateUID = templateUID;
  pushInVector(articles, numArticles, title, MAX_VECTOR_SIZE);
  if (!value.empty()) {
    pushInVector(articlesNE, numArticlesNE, title, fieldDef != FD_NO ? MAX_VECTOR_SIZE : 5000);
  } else {
    pushInVector(articlesE, numArticlesE, title, fieldDef != FD_NO ? MAX_VECTOR_SIZE : 5000);
  }
}

void FunctionInfo::addCall(const string& title) {
  pushInVector(articles, numArticles, title, MAX_VECTOR_SIZE);
}

TemplateInfo::TemplateInfo(const string& templateName, const string& templateCode,
                           const SideTemplateData& sideTemplateData)
    : m_templateName(templateName), m_numInclusions(0), m_numErrors(-1), m_numArticles(0), m_numArticlesNP(0),
      m_sideTemplateData(&sideTemplateData) {
  // HACK
  if (templateName.starts_with("Module:")) {
    m_fullPageName = templateName;
    m_namespace = TN_MODULE;
  } else {
    m_fullPageName = "Modèle:" + templateName;
    m_namespace = TN_TEMPLATE;
  }
  m_inLuaDB = m_sideTemplateData->isTemplateInLuaDB(templateName);
  if (m_inLuaDB && !containsInvoke(templateCode)) {
    CBL_WARNING << "Modèle enregistré dans la base de données lua, mais ne faisant pas appel à un module : '"
                << templateName << "'\n";
    m_inLuaDB = false;
  }
  if (m_namespace == TN_TEMPLATE) {
    extractVars(templateCode);
  }
}

class ParamEnumerator {
public:
  explicit ParamEnumerator(const wikicode::Template& template_) : template_(&template_), i(1), iParam(0) {}
  bool read(string& param, string& value) {
    if (i >= template_->size()) return false;
    template_->splitParamValue(i, &param, &value, wikicode::NORMALIZE_PARAM | wikicode::NORMALIZE_VALUE);
    if (param == wikicode::UNNAMED_PARAM) {
      iParam++;
      param = std::to_string(iParam);
    }
    i++;
    return true;
  }

private:
  const wikicode::Template* template_;
  int i;
  int iParam;
};

void TemplateInfo::readInclusion(Wiki& wiki, const string& title, const wikicode::Template& template_) {
  m_numInclusions++;
  pushInVector(m_articles, m_numArticles, title, MAX_VECTOR_SIZE);
  switch (m_namespace) {
    case TN_TEMPLATE:
      readTemplateInclusion(wiki, title, template_);
      break;
    case TN_MODULE:
      readModuleInclusion(title, template_);
      break;
  }
}

void TemplateInfo::readTemplateInclusion(Wiki& wiki, const string& title, const wikicode::Template& template_) {
  string unprefixedTitle(wiki.parseTitle(template_.name()).unprefixedTitle());
  RedirInfo& redirInfo = m_redirInfos[unprefixedTitle];
  pushInVector(redirInfo.articles, redirInfo.numArticles, title, MAX_VECTOR_SIZE);
  if (template_.size() == 1) {
    pushInVector(m_articlesNP, m_numArticlesNP, title, MAX_VECTOR_SIZE);
  }

  ParamEnumerator enumerator(template_);
  string param, value;
  if (m_nestedVariables.empty() && !m_inLuaDB) {
    while (enumerator.read(param, value)) {
      m_fieldInfos[param].addValue(title, value, m_numInclusions);
    }
  } else {
    map<string, string> fields;
    while (enumerator.read(param, value)) {
      // FIXME: pour les paramètres non nommés, il ne faudrait pas faire de trim (enfin pas tout de suite)
      fields[param] = value;
    }
    if (m_inLuaDB) {
      for (const string& param : m_sideTemplateData->getValidParams(m_templateName, fields)) {
        FieldInfo& fieldInfo = m_fieldInfos[param];
        if (fieldInfo.fieldDef == FD_NO) {
          fieldInfo.fieldDef = FD_LUA;
        }
      }
    }
    if (!m_nestedVariables.empty()) {
      // TODO: The support of nested variables is very hacky and it is hard to improve.
      // Either make sure it works well enough for the few templates relying on them and publish the code, or get rid
      // of this entirely.
    }
    for (const pair<const string, string>& field : fields) {
      m_fieldInfos[field.first].addValue(title, field.second, m_numInclusions);
    }
  }
}

void TemplateInfo::readModuleInclusion(const string& title, const wikicode::Template& template_) {
  string functionName;
  if (template_.size() >= 2) {
    functionName = template_[1].toString();
    wikicode::stripCommentsInPlace(functionName);
    functionName = string(cbl::trim(functionName));
  }
  if (functionName.find("{{") == string::npos) {
    if (functionName.empty()) {
      pushInVector(m_articlesNP, m_numArticlesNP, title, 5000);
    } else {
      m_functionInfos[functionName].addCall(title);
    }
  }
}

void TemplateInfo::generateTextInfo(FILE* file) {
  fprintf(file, "== [[%s]] ==\n", m_fullPageName.c_str());
  fprintf(file, "*inclusions : %i\n", m_numInclusions);
  fprintf(file, "*pages : %i\n", m_numArticles);

  for (const pair<const string, RedirInfo>& redir : m_redirInfos) {
    if (redir.first != m_templateName) {
      fprintf(file, "*pages utilisant la redirection [[Modèle:%s]] : %i\n", redir.first.c_str(),
              redir.second.numArticles);
    }
  }

  if (m_numArticlesNP > 0) {
    fprintf(file, "*pages utilisant le modèle sans paramètres : %i", m_numArticlesNP);
    if (m_numArticlesNP <= MAX_VECTOR_SIZE) {
      for (const string& article : m_articlesNP) {
        fprintf(file, " - [[%s]]", article.c_str());
      }
    }
    fprintf(file, "\n");
  }

  fprintf(file, "paramètres :\n");
  for (const pair<int, string>& itParam : m_paramByCount) {
    const string& param = itParam.second;
    FieldInfo& fi = m_fieldInfos[param];
    fprintf(file, "*%.100s (pages : %i, non vide : %i, existe : %s)", param.c_str(), fi.numArticles, fi.numArticlesNE,
            fi.fieldDef == FD_YES ? "oui" : (fi.fieldDef == FD_LUA ? "indirect" : "non"));
    if (fi.numArticlesNE <= MAX_VECTOR_SIZE) {
      for (const string& article : fi.articlesNE) {
        fprintf(file, " - [[%s]]", article.c_str());
      }
      if (fi.numArticles <= MAX_VECTOR_SIZE) {
        for (const string& article : fi.articlesE) {
          fprintf(file, " - [[%s]]", article.c_str());
        }
      }
    }
    fprintf(file, "\n");
  }
}

void TemplateInfo::generateJSONInfo(FILE* file) {
  JSONObject jsonInfo;
  jsonInfo.add("name", m_templateName);
  jsonInfo.add("inclusions", m_numInclusions);
  jsonInfo.add("pages", m_numArticles);
  if (m_namespace == TN_TEMPLATE) {
    JSONArray jsonRedirects;
    int numRedirects = 0;
    for (const auto& [redirectName, redirectInfo] : m_redirInfos) {
      if (redirectName != m_templateName) {
        JSONObject jsonRedirect;
        jsonRedirect.add("name", redirectName);
        jsonRedirect.add("pages", redirectInfo.numArticles);
        jsonRedirects.add(jsonRedirect);
        numRedirects++;
      }
    }
    if (numRedirects > 0) {
      jsonInfo.add("redirects", jsonRedirects);
    }

    JSONArray jsonParameters;
    const int MAX_PARAMS = 5000;
    int paramLimit = MAX_PARAMS;
    for (const pair<int, string>& countAndParam : m_paramByCount) {
      paramLimit--;
      if (paramLimit < 0) break;

      JSONObject jsonParameter;
      const string& param = countAndParam.second;
      string shortParam = cbl::legacyStringConv(cbl::utf8::substring(param, 0, 100));
      FieldInfo& fi = m_fieldInfos[param];

      jsonParameter.add("name", shortParam);
      jsonParameter.add("pages", fi.numArticles);
      jsonParameter.add("nonemptypages", fi.numArticlesNE);
      jsonParameter.add("valid", fi.fieldDef);

      if (fi.numArticlesNE <= MAX_VECTOR_SIZE) {
        JSONArray jsonExamples;
        unordered_set<string> firstList;
        for (const string& article : fi.articlesNE) {
          JSONObject jsonExample;
          jsonExample.add("title", article);
          jsonExample.add("type", "nonempty");
          jsonExamples.add(jsonExample);
          firstList.insert(article);
        }
        if (fi.numArticles <= MAX_VECTOR_SIZE) {
          for (const string& article : fi.articlesE) {
            if (firstList.find(article) == firstList.end()) {
              JSONObject jsonExample;
              jsonExample.add("title", article);
              jsonExample.add("type", "empty");
              jsonExamples.add(jsonExample);
            }
          }
        }
        jsonParameter.add("examples", jsonExamples);
      }
      jsonParameters.add(jsonParameter);
    }
    jsonInfo.add("parameters", jsonParameters);
    if (paramLimit < 0) {
      jsonInfo.add("parameters_more", m_paramByCount.size() - MAX_PARAMS);
    }
  } else if (m_namespace == TN_MODULE) {
    jsonInfo.add("namespace", "module");
    JSONArray jsonFunctions;
    const int MAX_FUNCTIONS = 5000;
    int functionLimit = MAX_FUNCTIONS;
    for (const pair<const string, FunctionInfo>& functionInfoIt : m_functionInfos) {
      functionLimit--;
      if (functionLimit < 0) break;

      string shortFunctionName = cbl::legacyStringConv(cbl::utf8::substring(functionInfoIt.first, 0, 100));
      const FunctionInfo& functionInfo = functionInfoIt.second;

      JSONObject jsonFunction;
      jsonFunction.add("name", shortFunctionName);
      jsonFunction.add("pages", functionInfo.numArticles);
      if (functionInfo.numArticles <= MAX_VECTOR_SIZE) {
        JSONArray jsonExamples;
        for (const string& article : functionInfo.articles) {
          jsonExamples.add(article);
        }
        jsonFunction.add("examples", jsonExamples);
      }

      jsonFunctions.add(jsonFunction);
    }
    jsonInfo.add("functions", jsonFunctions);
  }

  if (!m_articlesWithErrors.empty()) {
    JSONArray jsonErrors;
    const int MAX_ERRORS = 5000;
    int errorsLimit = MAX_ERRORS;
    for (const pair<const string, ArticleErrors>& article : m_articlesWithErrors) {
      errorsLimit--;
      if (errorsLimit < 0) break;

      JSONObject jsonError;
      string shortParam;
      jsonError.add("title", article.first);
      JSONArray jsonErrParams;
      for (const string& parameter : article.second.badParamsNE) {
        JSONObject jsonErrParam;
        shortParam = cbl::legacyStringConv(cbl::utf8::substring(parameter, 0, 100));
        jsonErrParam.add("name", shortParam);
        jsonErrParam.add("type", "nonempty");
        jsonErrParams.add(jsonErrParam);
      }
      for (const string& parameter : article.second.badParamsE) {
        JSONObject jsonErrParam;
        shortParam = cbl::legacyStringConv(cbl::utf8::substring(parameter, 0, 100));
        jsonErrParam.add("name", shortParam);
        jsonErrParam.add("type", "empty");
        jsonErrParams.add(jsonErrParam);
      }
      for (const string& parameter : article.second.dupParams) {
        JSONObject jsonErrParam;
        shortParam = cbl::legacyStringConv(cbl::utf8::substring(parameter, 0, 100));
        jsonErrParam.add("name", shortParam);
        jsonErrParam.add("type", "dup");
        jsonErrParams.add(jsonErrParam);
      }
      jsonError.add("parameters", jsonErrParams);
      jsonErrors.add(jsonError);
    }
    jsonInfo.add("errors", jsonErrors);
    if (errorsLimit < 0) {
      jsonInfo.add("errors_more", m_articlesWithErrors.size() - MAX_ERRORS);
    }
  }
  const string& str = jsonInfo.toString();
  fwrite(str.c_str(), 1, str.size(), file);
}

void TemplateInfo::generateInfo(FILE* file, OutputFormat format) {
  if (m_paramByCount.empty()) {
    for (const auto& [fieldName, fieldInfo] : m_fieldInfos) {
      if (fieldName.find("{{") == string::npos || fieldInfo.fieldDef != FD_NO) {
        m_paramByCount.push_back(make_pair(-fieldInfo.numArticles, fieldName));
      }
    }
    std::sort(m_paramByCount.begin(), m_paramByCount.end());
  }

  if (format != OF_TEXT && m_numErrors == -1) {
    if (m_namespace == TN_TEMPLATE) {
      for (const auto& [fieldName, fieldInfo] : m_fieldInfos) {
        if (fieldInfo.fieldDef != FD_NO) {
          for (const string& title : fieldInfo.articlesDup) {
            m_articlesWithErrors[title].dupParams.push_back(fieldName);
          }
        } else {
          for (const string& title : fieldInfo.articlesNE) {
            m_articlesWithErrors[title].badParamsNE.push_back(fieldName);
          }
          if (!RE2::FullMatch(fieldName, reNumericField)) {
            for (const string& title : fieldInfo.articlesE) {
              m_articlesWithErrors[title].badParamsE.push_back(fieldName);
            }
          }
        }
      }
    } else if (m_namespace == TN_MODULE) {
      for (const string& title : m_articlesNP) {
        m_articlesWithErrors.insert(make_pair(title, ArticleErrors()));
      }
    }
    m_numErrors = m_articlesWithErrors.size();
  }

  if (format == OF_TEXT) {
    generateTextInfo(file);
  } else if (format == OF_JSON) {
    generateJSONInfo(file);
  } else {
    CBL_FATAL << "TemplateInfo::generateInfo: Bad format";
  }
}

void TemplateInfo::extractVars(const string& templateCode) {
  string varName, varNameTr;
  wikicode::List parsedCode = wikicode::parse(templateCode);
  for (const wikicode::Variable& variable : parsedCode.getVariables()) {
    varName = variable.nameNode().toString();
    wikicode::stripCommentsInPlace(varName);
    varNameTr = string(cbl::trim(varName));
    if (varNameTr.empty()) {
      // In theory, the empty parameter "" is valid. However:
      // - There is no use of "" as a normal parameter on frwiki.
      // - The code of templates sometimes contains "{{{|safesubst:}}}". This is a trick to allow recursive
      //   substitution, but in that case, "" should not be considered as a valid parameter.
      // See https://fr.wikipedia.org/w/index.php?title=Discussion_utilisateur:Orlodrim&diff=187229253.
      continue;
    }
    FieldInfo& fieldInfo = m_fieldInfos[varNameTr];
    if (fieldInfo.fieldDef != FD_YES) {
      fieldInfo.fieldDef = FD_YES;
      if (varNameTr.find("{{") != string::npos) {
        m_nestedVariables.emplace_back(varNameTr, variable.nameNode().copy());
      }
    }
  }
}
