#ifndef CBL_HTML_ENTITIES_H
#define CBL_HTML_ENTITIES_H

#include <string>
#include <string_view>

namespace cbl {

// Replaces '&', '"', '<' and '>' with '&amp;', '&quot;', '&lt;' and '&gt;'.
// Nul chars are preserved.
std::string escapeHtml(std::string_view code);

// Unescapes known HTML entities.
// This unescapes many more entities than those escaped by escapeHtml, e.g. '&eacute;' becomes 'é'.
// unescapeHtml(escapeHtml(code)) == code but the opposite is not true.
// Nul chars are preserved but none are added. In particular, &#0; is not unescaped to nul.
std::string unescapeHtml(std::string_view escapedCode);

}  // namespace cbl

#endif
