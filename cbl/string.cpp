#include "string.h"
#include <ctype.h>
#include <errno.h>
#include <algorithm>
#include <climits>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "error.h"

using std::string;
using std::string_view;
using std::vector;

namespace cbl {
namespace string_internal {

const signed char INT_OF_HEX_DIGIT[0x100] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
    -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

}  // namespace string_internal

constexpr char HEX_DIGITS[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

enum ParseIntResult {
  PARSE_INT_OK,
  PARSE_INT_TOO_SMALL,
  PARSE_INT_TOO_LARGE,
  PARSE_INT_INVALID,
};

static bool startsWithOptionalSignAndDigit(const string& s) {
  const char* firstDigit = s.c_str();
  if (*firstDigit == '-') firstDigit++;
  return *firstDigit >= '0' && *firstDigit <= '9';
}

static ParseIntResult tryParseInt(const string& s, int min, int max, int* result) {
  if (!startsWithOptionalSignAndDigit(s)) return PARSE_INT_INVALID;
  char* end = nullptr;
  errno = 0;
  long longResult = strtol(s.c_str(), &end, 10);
  if (*end != '\0') return PARSE_INT_INVALID;
  if (errno != 0) {
    return longResult == LONG_MIN ? PARSE_INT_TOO_SMALL : PARSE_INT_TOO_LARGE;
  }
  if (longResult < min) {
    return PARSE_INT_TOO_SMALL;
  } else if (longResult > max) {
    return PARSE_INT_TOO_LARGE;
  }
  *result = static_cast<int>(longResult);
  return PARSE_INT_OK;
}

int parseInt(const string& s) {
  int result;
  if (tryParseInt(s, INT_MIN, INT_MAX, &result) != PARSE_INT_OK) {
    throw ParseError("Invalid integer '" + s + "'");
  }
  return result;
}

int parseIntInRange(const string& s, int min, int max, int defValue, int options) {
  if (min > max) {
    throw std::invalid_argument("Invalid range for parseIntInRange");
  }
  int result;
  switch (tryParseInt(s, min, max, &result)) {
    case PARSE_INT_OK:
      return result;
    case PARSE_INT_TOO_SMALL:
      return (options & MIN_IF_TOO_SMALL) ? min : defValue;
    case PARSE_INT_TOO_LARGE:
      return (options & MAX_IF_TOO_LARGE) ? max : defValue;
    case PARSE_INT_INVALID:
      break;
  }
  return defValue;
}

int64_t parseInt64(const string& s) {
  if (startsWithOptionalSignAndDigit(s)) {
    char* end = nullptr;
    errno = 0;
    int64_t result = strtoll(s.c_str(), &end, 10);
    if (errno == 0 && *end == '\0') {
      return result;
    }
  }
  throw cbl::ParseError("Invalid int64 '" + s + "'");
}

StringBorders getTrimmedBorders(string_view s, int trimOptions) {
  const char* start = s.data();
  const char* end = start + s.size();
  if (trimOptions & TRIM_LEFT) {
    for (; start < end && isspace(static_cast<unsigned char>(*start)); start++) {
    }
  }
  if (trimOptions & TRIM_RIGHT) {
    for (; start < end && isspace(static_cast<unsigned char>(*(end - 1))); end--) {
    }
  }
  return {start - s.data(), end - s.data()};
}

string_view trim(string_view s, int trimOptions) {
  StringBorders borders = getTrimmedBorders(s, trimOptions);
  return s.substr(borders.left, borders.right - borders.left);
}

bool isSpace(string_view s) {
  return trim(s).empty();
}

string collapseSpace(string_view s) {
  string result;
  result.reserve(s.size());
  bool spaceInBuffer = false;
  for (char c : s) {
    if (isspace(static_cast<unsigned char>(c))) {
      spaceInBuffer = true;
    } else {
      if (spaceInBuffer) {
        result += ' ';
        spaceInBuffer = false;
      }
      result += c;
    }
  }
  if (spaceInBuffer) {
    result += ' ';
  }
  return result;
}

string trimAndCollapseSpace(string_view s) {
  return collapseSpace(trim(s));
}

string toLowerCaseASCII(string_view s) {
  string result(s);
  for (char& c : result) {
    if (c >= 'A' && c <= 'Z') {
      c += 'a' - 'A';
    }
  }
  return result;
}

FieldGenerator::FieldGenerator(string_view str, char separator, bool ignoreLastFieldIfEmpty)
    : m_unconsumedPart(str), m_separator(separator) {
  if (ignoreLastFieldIfEmpty) {
    if (m_unconsumedPart.empty()) {
      m_atEnd = true;
    } else if (m_unconsumedPart.back() == separator) {
      m_unconsumedPart.remove_suffix(1);
    }
  }
}

bool FieldGenerator::next() {
  if (m_atEnd) {
    return false;
  }
  size_t separatorPosition = m_unconsumedPart.find(m_separator);
  m_value = m_unconsumedPart.substr(0, separatorPosition);
  if (separatorPosition == string_view::npos) {
    m_atEnd = true;
  } else {
    m_unconsumedPart.remove_prefix(separatorPosition + 1);
  }
  return true;
}

vector<string_view> splitAsVector(string_view str, char separator, bool ignoreLastFieldIfEmpty) {
  vector<string_view> result;
  for (string_view field : split(str, separator, ignoreLastFieldIfEmpty)) {
    result.push_back(field);
  }
  return result;
}

vector<string_view> splitLinesAsVector(string_view str) {
  return splitAsVector(str, '\n', true);
}

static int replaceCat(string_view text, string_view oldFragment, string_view newFragment, string& buffer) {
  if (oldFragment.empty()) {
    throw std::invalid_argument("Cannot replace an empty string");
  }
  int fragmentCount = 0;
  while (true) {
    size_t fragmentPosition = text.find(oldFragment);
    if (fragmentPosition == string_view::npos) break;
    fragmentCount++;
    buffer += text.substr(0, fragmentPosition);
    buffer += newFragment;
    text.remove_prefix(fragmentPosition + oldFragment.size());
  }
  buffer += text;
  return fragmentCount;
}

string replace(string_view text, string_view oldFragment, string_view newFragment) {
  string result;
  replaceCat(text, oldFragment, newFragment, result);
  return result;
}

int replaceInPlace(string& text, string_view oldFragment, string_view newFragment) {
  if (oldFragment.empty()) {
    throw std::invalid_argument("Cannot replace an empty string");
  }
  size_t firstOccurrence = text.find(oldFragment);
  if (firstOccurrence == string::npos) {
    return 0;
  } else if (oldFragment.size() == 1 && newFragment.size() == 1) {
    int count = 0;
    char oldChar = oldFragment[0];
    char newChar = newFragment[0];
    for (string::iterator it = text.begin() + firstOccurrence; it != text.end(); ++it) {
      if (*it == oldChar) {
        *it = newChar;
        count++;
      }
    }
    return count;
  } else {
    string remainingText = text.substr(firstOccurrence + oldFragment.size());
    text.resize(firstOccurrence);
    text += newFragment;
    return replaceCat(remainingText, oldFragment, newFragment, text) + 1;
  }
}

void encodeURIComponentCat(string_view str, string& buffer) {
  static const bool* const CHARS_TO_ENCODE = []() {
    static bool charsToEncode[0x100];
    std::fill(charsToEncode, charsToEncode + 0x100, true);
    constexpr string_view UNENCODED_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~*\'()";
    for (unsigned char c : UNENCODED_CHARS) {
      charsToEncode[c] = false;
    }
    return charsToEncode;
  }();

  size_t requiredCapacity = buffer.size() + str.size();
  for (unsigned char c : str) {
    if (CHARS_TO_ENCODE[c]) {
      requiredCapacity += 2;
    }
  }
  if (buffer.capacity() < requiredCapacity) {
    buffer.reserve(requiredCapacity);
  }
  for (unsigned char c : str) {
    if (CHARS_TO_ENCODE[c]) {
      buffer += '%';
      buffer += HEX_DIGITS[(c >> 4) & 0xF];
      buffer += HEX_DIGITS[c & 0xF];
    } else {
      buffer += static_cast<char>(c);
    }
  }
}

string encodeURIComponent(string_view str) {
  string result;
  encodeURIComponentCat(str, result);
  return result;
}

string decodeURIComponent(string_view str) {
  string result;
  result.reserve(str.size());
  size_t size = str.size();
  for (size_t i = 0; i < size; i++) {
    char c = str[i];
    if (c == '%' && i + 2 < size) {
      int h1 = intOfHexDigit(str[i + 1]);
      int h2 = intOfHexDigit(str[i + 2]);
      if (h1 != -1 && h2 != -1 && h1 + h2 > 0) {
        i += 2;
        result += static_cast<char>((h1 << 4) + h2);
      } else {
        result += '%';
      }
    } else {
      result += c;
    }
  }
  return result;
}

string shellEscape(string_view str) {
  string result;
  result += '\'';
  replaceCat(str, "'", R"('"'"')", result);
  result += '\'';
  return result;
}

static int getLineIndentation(string_view line) {
  int lineLength = line.size();
  for (int indent = 0; indent < lineLength; indent++) {
    if (line[indent] != ' ') {
      return indent;
    }
  }
  return INT_MAX;
}

string unindent(string_view s) {
  int minIndent = INT_MAX;
  for (string_view line : split(s, '\n')) {
    minIndent = std::min(minIndent, getLineIndentation(line));
  }
  string result;
  result.reserve(s.size());
  for (string_view line : split(s, '\n')) {
    if (!result.empty()) {
      result += '\n';
    }
    if (minIndent < static_cast<int>(line.size())) {
      result += line.substr(minIndent);
    }
  }
  return result;
}

}  // namespace cbl
