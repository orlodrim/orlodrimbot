#include "titles.h"
#include <re2/re2.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "cbl/file.h"
#include "cbl/log.h"
#include "mwclient/wiki.h"
#include "process.h"

using std::string;

namespace dump_processing {

void Titles::prepare() {
  string disambigRegexp = cbl::readFile(getAbsolutePath(getParameter("input_disambigregexp")));
  CBL_ASSERT(disambigRegexp.find('\n') == string::npos);
  m_reDisambiguation.reset(new re2::RE2(disambigRegexp));
  openMainOutputFileFromParam("output");
}

void Titles::processPage(Page& page) {
  static const re2::RE2 rePortal(R"(\{\{\s*[Pp]ortail(\s|\|))");
  static const re2::RE2 reCategory(R"((?i:\[\[\s*(Catégorie|Category)\s*:))");
  static const re2::RE2 reEvaluation(R"(\{\{\s*[Ww]ikiprojet\s*[|}])");
  static const re2::RE2 reNonEmptyTodo(R"(\{\{(\s|\n)*([Àà] +faire|[Tt]odo|[Tt]ODO)(\s|\n)*\|(\s|\n)*[^\s\n\}])");
  static const re2::RE2 reEmptyTodo(R"(\{\{(\s|\n)*([Àà] +faire|[Tt]odo|[Tt]ODO)(\s|\n)*(\|(\s|\n)*)?\}\})");

  const string& code = page.code();
  int namespace_ = page.namespace_();
  string redirTarget, redirAnchor;
  bool isRedirect = environment().wiki().readRedirect(code, &redirTarget, &redirAnchor);
  bool isDisambiguation = RE2::PartialMatch(code, *m_reDisambiguation);
  bool hasPortal = namespace_ == mwc::NS_MAIN && RE2::PartialMatch(code, rePortal);
  bool hasCat = RE2::PartialMatch(code, reCategory);
  bool hasEval = namespace_ == mwc::NS_TALK && RE2::PartialMatch(code, reEvaluation);
  bool hasNonEmptyTodo = namespace_ == mwc::NS_TALK && RE2::PartialMatch(code, reNonEmptyTodo);
  bool hasEmptyTodo = namespace_ == mwc::NS_TALK && RE2::PartialMatch(code, reEmptyTodo);

  string properties;
  properties.reserve(strlen("|RHPCEtT||") + redirTarget.size() + redirAnchor.size());
  properties += '|';
  if (isRedirect) properties += 'R';
  if (isDisambiguation) properties += 'H';
  if (hasPortal) properties += 'P';
  if (hasCat) properties += 'C';
  if (hasEval) properties += 'E';
  if (hasNonEmptyTodo) properties += 'T';
  if (hasEmptyTodo) properties += 't';
  if (isRedirect) {
    properties += '|';
    properties += environment().wiki().parseTitle(redirTarget, mwc::NS_MAIN, mwc::PTF_LINK_TARGET).title;
    if (!redirAnchor.empty()) {
      properties += '|';
      properties += redirAnchor;
    }
  }

  fprintf(mainOutputFile(), "%s|%i%s\n", page.title().c_str(), (int) code.size(),
          properties.size() > 1 ? properties.c_str() : "");
}

}  // namespace dump_processing
