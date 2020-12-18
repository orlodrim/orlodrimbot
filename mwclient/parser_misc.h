#ifndef MWC_PARSER_MISC_H
#define MWC_PARSER_MISC_H

#include <string>
#include <string_view>

namespace wikicode {

// Returns true if s only contains whitespace and comments (the last comment may be unclosed).
bool isSpaceOrComment(std::string_view s);

// Removes comments from s.
// This function is not as accurate as the full parser, e.g. it removes <!-- ... --> even in <nowiki> tags.
std::string stripComments(std::string_view s);
void stripCommentsInPlace(std::string& s);

// Adds <nowiki> tags around `code`. Inside `code`, replaces "&" and "<" escaped with HTML entities.
std::string escape(std::string_view code);

// Returns the number of "=" around `line` (1 for "= Title =", 2 for "== Title ==", etc.).
// Tries to reproduce MediaWiki behavior in special cases (e.g. returns 2 for "== Title ===").
// If `line` is not a title, returns 0.
int getTitleLevel(std::string_view line);

// Strips '=' signs around the title `line` (always the same number on both side), and then strips whitespace.
// Example: getTitleContent("== Some title ==") => "Some title".
std::string getTitleContent(std::string_view line);

}  // namespace wikicode

#endif
