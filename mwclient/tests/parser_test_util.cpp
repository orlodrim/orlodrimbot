#include "parser_test_util.h"
#include <stdexcept>
#include <string>
#include "mwclient/parser.h"

using std::string;

namespace wikicode {

const char* getNodeTypeString(NodeType type) {
  switch (type) {
    case NT_LIST:
      return "list";
    case NT_TEXT:
      return "text";
    case NT_COMMENT:
      return "comment";
    case NT_TAG:
      return "tag";
    case NT_LINK:
      return "link";
    case NT_TEMPLATE:
      return "template";
    case NT_VARIABLE:
      return "var";
  }
  throw std::invalid_argument("Invalid node type " + std::to_string(type));
}

string getNodeDebugString(const Node& node) {
  string debugString;
  debugString += getNodeTypeString(node.type());
  debugString += "(";
  switch (node.type()) {
    case NT_LIST: {
      const List& list = node.asList();
      for (int i = 0; i < list.size(); i++) {
        if (i > 0) debugString += ",";
        debugString += getNodeDebugString(list[i]);
      }
      break;
    }
    case NT_TEXT:
      debugString += node.asText().text;
      break;
    case NT_COMMENT:
      debugString += node.asComment().text;
      break;
    case NT_TAG: {
      const Tag& tag = node.asTag();
      debugString += tag.openingTag();
      if (tag.content() || !tag.closingTag().empty()) {
        debugString += ",";
      }
      if (tag.content()) {
        debugString += getNodeDebugString(*tag.content());
      }
      if (!tag.closingTag().empty()) {
        debugString += ",";
        debugString += tag.closingTag();
      }
      break;
    }
    case NT_LINK: {
      const Link& link = node.asLink();
      for (int i = 0; i < link.size(); i++) {
        if (i > 0) debugString += ",";
        debugString += getNodeDebugString(link[i]);
      }
      break;
    }
    case NT_TEMPLATE: {
      const Template& template_ = node.asTemplate();
      for (int i = 0; i < template_.size(); i++) {
        if (i > 0) debugString += ",";
        debugString += getNodeDebugString(template_[i]);
      }
      break;
    }
    case NT_VARIABLE: {
      const Variable& var = node.asVariable();
      debugString += getNodeDebugString(var.nameNode());
      if (var.defaultValue()) {
        debugString += ",";
        debugString += getNodeDebugString(*var.defaultValue());
      }
      break;
    }
  }
  debugString += ")";
  return debugString;
}

}  // namespace wikicode
