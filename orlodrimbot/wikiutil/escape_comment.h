#ifndef WIKIUTIL_ESCAPE_COMMENT_H
#define WIKIUTIL_ESCAPE_COMMENT_H

#include <string>
#include <string_view>
#include "mwclient/wiki.h"

namespace wikiutil {

// Converts an edit comment to normal wikitext.
// An edit comment can contain links, but links to categories and files are not special. Templates and external links
// don't work in this context.
// Example: escapeComment("Deleting [[File:X]]") = "Deleting [[:File:X]]".
std::string escapeComment(const mwc::Wiki& wiki, std::string_view comment);

}  // namespace wikiutil

#endif
