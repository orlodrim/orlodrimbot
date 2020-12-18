// Wikicode parsing. This parses comments, tags, links, templates, and variables from a string.
// See parser_nodes.h for details about the structure of the result.
//
// The complexity is linear in the size of the input.
// No template expansion or other computation depending on external data is performed, which means that the parsing
// cannot be fully accurate. For instance, it cannot resolve the name of the template in an expression like
// "{{Data/{{{1}}}}}" and it cannot guess that "[[Wikipedia{{)!}}{{)!}}" produces the link [[Wikipedia]].
// There is intentionally no dependency between the parser and the Wiki class. The parser makes no assumption about
// existing namespaces and their configuration.
// In LENIENT mode, the parser tries hard to handle errors in the same way as the MediaWiki, but there are some corner
// with some differences (see parser_test.cpp). In any case, calling toString() on the parsed result always returns the
// original string.
//
// Parsed nodes satisfy the following conditions:
// - Direct children of lists are not lists.
// - Lists do not contain consecutive text nodes.
// - Text nodes are never empty.
// - Links and templates always have at least one field.
// However, there is no requirement to maintain them when manipulating nodes outside of the parser.
#ifndef MWC_PARSER_H
#define MWC_PARSER_H

#include <string_view>
#include "cbl/error.h"
#include "parser_misc.h"
#include "parser_nodes.h"

namespace wikicode {

// Error class for the STRICT mode.
class ParseError : public cbl::ParseError {
public:
  using cbl::ParseError::ParseError;
};

// In LENIENT mode, parse never throws a ParseError. Structures that cannot be parsed are considered as text.
// In STRICT mode, a ParseError is thrown for any unmatched opening or closing token. It is only recommended to detect
// errors or when it is critical that parsing is done in the same way as MediaWiki (e.g. risky edits in the template
// namespace). It can reject code that is arguably not broken, such as "{1, 4, 9, ...n{{2}}}".
enum ErrorLevel { LENIENT, STRICT };

// Parses wikicode in the range [codeBegin, codeEnd).
// Prefer using the version below that takes a string_view.
List parse(const char* codeBegin, const char* codeEnd, ErrorLevel level = LENIENT);

// Parses `code` as wikicode.
// This version takes a string_view and thus can be called on std::string and const char* by implicit conversion.
inline List parse(std::string_view code, ErrorLevel level = LENIENT) {
  return parse(code.data(), code.data() + code.size(), level);
}

namespace parser_internal {

// Exposed for testing purposes only.
int getCodeDepth(std::string_view code);
int setParserMaxDepth(int maxDepth);

}  // namespace parser_internal
}  // namespace wikicode

#endif
