// IMPLEMENTS: wiki.h
#include <string>
#include "cbl/http_client.h"
#include "cbl/json.h"
#include "request.h"
#include "wiki.h"
#include "wiki_defs.h"

using std::string;

namespace mwc {

string Wiki::expandTemplates(const string& code, const string& title) {
  WikiRequest request("expandtemplates");
  request.setMethod(WikiRequest::METHOD_POST_NO_SIDE_EFFECT);
  request.setParam("title", title);
  request.setParam("text", code);
  request.setParam("prop", "wikitext");

  try {
    json::Value answer = request.run(*this);
    const json::Value& expandedCodeValue = answer["expandtemplates"]["wikitext"];
    if (expandedCodeValue.isNull()) {
      throw UnexpectedAPIResponseError("expandtemplates.wikitext missing in server answer");
    }
    return expandedCodeValue.str();
  } catch (WikiError& error) {
    error.addContext("Cannot expand templates");
    throw;
  }
}

string Wiki::renderAsHTML(const RenderParams& params) {
  WikiRequest request("parse");
  request.setMethod(WikiRequest::METHOD_POST_NO_SIDE_EFFECT);
  request.setParam("title", params.title);
  request.setParam("text", params.text);
  request.setParam("prop", "text");
  request.setOrClearParam("disableeditsection", "1", params.disableEditSection);
  request.setParam("disablelimitreport", 1);
  request.setParam("contentmodel", "wikitext");
  request.setParam("wrapoutputclass", "");

  try {
    json::Value answer = request.run(*this);
    const json::Value& parsedTextNode = answer["parse"]["text"]["*"];
    if (parsedTextNode.isNull()) {
      throw UnexpectedAPIResponseError("parse.text.* missing in server answer");
    }
    return parsedTextNode.str();
  } catch (WikiError& error) {
    error.addContext("Cannot parse text");
    throw;
  }
}

}  // namespace mwc
