#include "include_tags.h"
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "cbl/string.h"

using std::optional;
using std::string;
using std::string_view;
using std::vector;

namespace mwc {
namespace include_tags {

static optional<TagName> stringToTagName(string_view str) {
  if (str == "includeonly") {
    return TagName::INCLUDEONLY;
  } else if (str == "noinclude") {
    return TagName::NOINCLUDE;
  } else if (str == "onlyinclude") {
    return TagName::ONLYINCLUDE;
  } else if (str == "nowiki") {
    return TagName::NOWIKI;
  } else if (str == "pre") {
    return TagName::PRE;
  }
  return std::nullopt;
}

static bool findNextTag(string_view code, size_t start, size_t& tagBegin, size_t& tagEnd, Tag& tag) {
  size_t position = start;
  while (true) {
    tagBegin = code.find('<', position);
    if (tagBegin == string_view::npos) {
      // tagBegin is intentionally left to the value string_view::npos in that case.
      return false;
    } else if (code.substr(tagBegin, 4) == "<!--") {
      tagEnd = tagBegin + 4;
      tag.name = TagName::COMMENT;
      tag.type = TagType::OPENING;
      return true;
    }
    size_t lastTagChar = code.find_first_of("<>\n", tagBegin + 1);
    if (lastTagChar == string::npos) {
      position = code.size();
      continue;
    } else if (code[lastTagChar] != '>') {
      position = lastTagChar;
      continue;
    }
    tagEnd = lastTagChar + 1;
    TagType tagType = TagType::OPENING;
    if (code[tagBegin + 1] == '/') {
      tagType = TagType::CLOSING;
    } else if (code[lastTagChar - 1] == '/') {
      tagType = TagType::SELF_CLOSING;
    }
    size_t tagNameStart = tagBegin + (tagType == TagType::CLOSING ? 2 : 1);
    size_t tagNameEnd = code.find_first_of(" />", tagNameStart);
    string tagNameString = cbl::toLowerCaseASCII(code.substr(tagNameStart, tagNameEnd - tagNameStart));
    optional<TagName> tagName = stringToTagName(tagNameString);
    if (tagName) {
      tag.name = *tagName;
      tag.type = tagType;
      return true;
    }
    position = tagEnd;
  }
}

void enumIncludeTags(string_view code, const ParseCallback& parseCallback, const ErrorCallback& errorCallback) {
  bool ignoreNextOpeningTags[TAG_NAME_MAX] = {false};
  size_t tagBegin, tagEnd;
  Tag tag;
  bool inRawTextTag = false;
  TagName rawTextTagName = TagName::INCLUDEONLY;
  size_t rawTextTagEnd = 0;
  size_t tokenStart = 0;
  for (size_t position = 0; findNextTag(code, position, tagBegin, tagEnd, tag) || inRawTextTag; position = tagEnd) {
    if (inRawTextTag) {
      if (tagBegin == string_view::npos) {
        errorCallback(UNCLOSED_TAG, rawTextTagName, std::nullopt);
        ignoreNextOpeningTags[static_cast<int>(rawTextTagName)] = true;
        tagEnd = rawTextTagEnd;
        inRawTextTag = false;
      } else if (tag.name == rawTextTagName && tag.type == TagType::CLOSING) {
        inRawTextTag = false;
      }
    } else if (tag.name == TagName::INCLUDEONLY || tag.name == TagName::NOINCLUDE || tag.name == TagName::ONLYINCLUDE) {
      if (tokenStart < tagBegin) {
        parseCallback(code.substr(tokenStart, tagBegin - tokenStart), nullptr);
      }
      parseCallback(code.substr(tagBegin, tagEnd - tagBegin), &tag);
      tokenStart = tagEnd;
    } else if (tag.name == TagName::COMMENT) {
      // tag.type must be TagType::OPENING.
      tagEnd = code.find("-->", tagEnd);
      if (tagEnd == string::npos) {
        errorCallback(UNCLOSED_COMMENT, std::nullopt, std::nullopt);
        tagEnd = code.size();
      }
    } else if (tag.type == TagType::OPENING && !ignoreNextOpeningTags[static_cast<int>(tag.name)]) {
      inRawTextTag = true;
      rawTextTagName = tag.name;
      rawTextTagEnd = tagEnd;
    } else if (tag.type == TagType::CLOSING) {
      errorCallback(UNOPENED_TAG, tag.name, std::nullopt);
    }
  }
  if (tokenStart < code.size()) {
    parseCallback(code.substr(tokenStart), nullptr);
  }
}

void parse(string_view code, std::string* notTranscluded, std::string* transcluded,
           const ErrorCallback& errorCallback) {
  bool isTagOpen[TAG_NAME_MAX] = {false};
  vector<TagName> openTags;
  bool withOnlyInclude = false;

  if (notTranscluded) {
    notTranscluded->clear();
  }
  if (transcluded) {
    transcluded->clear();
  }

  enumIncludeTags(
      code,
      [&](string_view token, const Tag* tag) {
        bool addAsText = true;
        if (tag) {
          addAsText = false;
          if (tag->type == TagType::OPENING) {
            if (isTagOpen[static_cast<int>(tag->name)]) {
              errorCallback(NESTED_OPEN_OPEN, tag->name, std::nullopt);
            } else {
              isTagOpen[static_cast<int>(tag->name)] = true;
              openTags.push_back(tag->name);
            }
            if (tag->name == TagName::ONLYINCLUDE && !withOnlyInclude) {
              if (transcluded) {
                transcluded->clear();
              }
              withOnlyInclude = true;
            }
          } else if (tag->type == TagType::CLOSING) {
            if (isTagOpen[static_cast<int>(tag->name)]) {
              isTagOpen[static_cast<int>(tag->name)] = false;
              if (openTags.empty()) {
                // Internal inconsistency, but it is safe to ignore.
              } else if (openTags.back() == tag->name) {
                openTags.pop_back();
              } else {
                errorCallback(OPEN_CLOSE_MISMATCH, openTags.back(), tag->name);
                openTags.erase(std::remove(openTags.begin(), openTags.end(), tag->name), openTags.end());
              }
            } else {
              errorCallback(UNOPENED_TAG, tag->name, std::nullopt);
              addAsText = true;
            }
          } else if (tag->type == TagType::SELF_CLOSING) {
            if (isTagOpen[static_cast<int>(tag->name)]) {
              errorCallback(NESTED_OPEN_AUTOCLOSE, tag->name, std::nullopt);
            }
          }
        }
        if (addAsText) {
          if (notTranscluded && !isTagOpen[static_cast<int>(TagName::INCLUDEONLY)]) {
            *notTranscluded += token;
          }
          if (transcluded && !isTagOpen[static_cast<int>(TagName::NOINCLUDE)] &&
              (!withOnlyInclude || isTagOpen[static_cast<int>(TagName::ONLYINCLUDE)])) {
            *transcluded += token;
          }
          if (isTagOpen[static_cast<int>(TagName::INCLUDEONLY)] && isTagOpen[static_cast<int>(TagName::NOINCLUDE)]) {
            errorCallback(INCLUDEONLY_AND_NOINCLUDE, std::nullopt, std::nullopt);
            // TODO: Same detection for onlyinclude. To make this work, we have to know from the beginning if the
            // code contains a <onlyinclude> tag.
          }
        }
      },
      errorCallback);

  if (!openTags.empty()) {
    errorCallback(UNCLOSED_TAG, openTags.back(), std::nullopt);
  }
}

}  // namespace include_tags
}  // namespace mwc
