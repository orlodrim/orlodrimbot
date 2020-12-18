#ifndef MWC_PARSER_TEST_UTIL_H
#define MWC_PARSER_TEST_UTIL_H

#include <string>
#include "mwclient/parser.h"

namespace wikicode {

const char* getNodeTypeString(NodeType type);
std::string getNodeDebugString(const Node& node);

}  // namespace wikicode

#endif
