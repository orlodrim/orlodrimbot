// Library to parse <includeonly>, <noinclude> and <onlyinclude> tags.
#ifndef MWC_UTIL_INCLUDE_TAGS_H
#define MWC_UTIL_INCLUDE_TAGS_H

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include "cbl/error.h"

namespace mwc {
namespace include_tags {

enum class TagName {
  INCLUDEONLY,
  NOINCLUDE,
  ONLYINCLUDE,
  // ParseCallback is never called with the following tags.
  NOWIKI,
  PRE,
  COMMENT,
};

constexpr int TAG_NAME_MAX = static_cast<int>(TagName::COMMENT) + 1;

enum class TagType {
  OPENING,
  CLOSING,
  SELF_CLOSING,
};

struct Tag {
  TagName name;
  TagType type;
};

enum ErrorType {
  UNCLOSED_COMMENT,
  UNCLOSED_TAG,
  UNOPENED_TAG,
  NESTED_OPEN_OPEN,
  NESTED_OPEN_AUTOCLOSE,
  OPEN_CLOSE_MISMATCH,
  INCLUDEONLY_AND_NOINCLUDE,
};

using ErrorCallback = std::function<void(ErrorType type, std::optional<TagName> tag1, std::optional<TagName> tag2)>;

inline void ignoreErrors(ErrorType type, std::optional<TagName> tag1, std::optional<TagName> tag2) {}

// Computes code as it appears when it is not transcluded (remove <includeonly> sections) and when it is
// transcluded (remove <noinclude> sections + take <onlyinclude> into account).
void parse(std::string_view code, std::string* notTranscluded, std::string* transcluded,
           const ErrorCallback& errorCallback = ignoreErrors);

using ParseCallback = std::function<void(std::string_view token, const Tag* tag)>;

// Enumerates all include tags and the text between them.
void enumIncludeTags(std::string_view code, const ParseCallback& parseCallback, const ErrorCallback& = ignoreErrors);

}  // namespace include_tags
}  // namespace mwc

#endif
