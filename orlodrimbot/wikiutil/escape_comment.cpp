#include "escape_comment.h"
#include <string>
#include <string_view>
#include "cbl/string.h"
#include "mwclient/wiki.h"

using mwc::Wiki;
using std::string;
using std::string_view;

namespace wikiutil {

static string escapeCommentFragment(string_view fragment) {
  if (fragment.find_first_of("<>[]{}") != string_view::npos || fragment.find("://") != string_view::npos ||
      fragment.find("''") != string_view::npos) {
    return "<nowiki>" + string(fragment) + "</nowiki>";
  } else {
    return string(fragment);
  }
}

string escapeComment(const Wiki& wiki, string_view comment) {
  string newComment;
  string commentHTML = cbl::trimAndCollapseSpace(cbl::replace(comment, "<", "&lt;"));
  string_view textToParse = commentHTML;
  while (true) {
    size_t linkStart = textToParse.find("[[");
    if (linkStart == string_view::npos) break;
    size_t linkContentStart = linkStart + 2;
    size_t linkContentEnd = textToParse.find("]]", linkContentStart);
    if (linkContentEnd == string_view::npos) break;

    string_view linkContent = textToParse.substr(linkContentStart, linkContentEnd - linkContentStart);
    size_t pipePosition = linkContent.find('|');
    string_view linkTarget = cbl::trim(linkContent.substr(0, pipePosition));  // Also works if pipePosition == npos.
    if (linkTarget.find_first_of(">[]{}") == string_view::npos && linkTarget.find("&lt;") == string_view::npos) {
      if (linkStart > 0) {
        newComment += escapeCommentFragment(textToParse.substr(0, linkStart));
      }
      newComment += "[[";
      int namespace_ = wiki.getTitleNamespace(linkTarget);
      if (!linkTarget.starts_with(":") && (namespace_ == mwc::NS_CATEGORY || namespace_ == mwc::NS_FILE)) {
        // NOTE: This does not generate a valid link for "[[_:Category:A]]", but this is an edge case.
        newComment += ":";
      }
      newComment += linkTarget;
      if (pipePosition != string_view::npos) {
        newComment += '|';
        newComment += escapeCommentFragment(linkContent.substr(pipePosition + 1));
      }
      newComment += "]]";
    } else {
      newComment += escapeCommentFragment(textToParse.substr(0, linkContentEnd + 2));
    }
    textToParse.remove_prefix(linkContentEnd + 2);
  }
  if (!textToParse.empty()) {
    newComment += escapeCommentFragment(textToParse);
  }
  return newComment;
}

}  // namespace wikiutil
