#ifndef CBL_STRING_H
#define CBL_STRING_H

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include "generated_range.h"

namespace cbl {
namespace string_internal {

extern const signed char INT_OF_HEX_DIGIT[0x100];

constexpr size_t getTotalLength() {
  return 0;
}

template <typename... Args>
size_t getTotalLength(std::string_view firstArg, Args... args) {
  return firstArg.size() + getTotalLength(args...);
}

constexpr void concatHelper(std::string& buffer) {}

template <typename... Args>
void concatHelper(std::string& buffer, std::string_view firstArg, Args... args) {
  buffer += firstArg;
  concatHelper(buffer, args...);
}

}  // namespace string_internal

// Intended to identify place where a temporary string_view -> string conversion was introduced in a function call, but
// should go away as the called function is updated to take a string_view.
using legacyStringConv = std::string;

inline bool startsWith(std::string_view s, std::string_view prefix) {
  size_t n = prefix.size();
  return n <= s.size() && memcmp(s.data(), prefix.data(), n) == 0;
}

inline bool endsWith(std::string_view s, std::string_view suffix) {
  size_t n = suffix.size();
  return n <= s.size() && memcmp(s.data() + s.size() - n, suffix.data(), n) == 0;
}

// Concatenates multiple string_views (or anything convertible to string_view).
template <typename... Args>
std::string concat(Args... args) {
  std::string result;
  result.reserve(string_internal::getTotalLength(args...));
  string_internal::concatHelper(result, args...);
  return result;
}

// Appends multiple string_views (or anything convertible to string_view) to buffer.
template <typename... Args>
void append(std::string& buffer, Args... args) {
  size_t newLength = buffer.size() + string_internal::getTotalLength(args...);
  if (newLength > buffer.capacity()) {
    buffer.reserve(newLength);
  }
  string_internal::concatHelper(buffer, args...);
}

// Parses s as an int represented in base 10.
// Strict parsing (space, '+' sign or extra characters at the end are not allowed). Leading zeros are ignored.
// Throws: ParseError.
int parseInt(const std::string& s);

// Parses s as an int64_t in the same way as parseInt.
int64_t parseInt64(const std::string& s);

enum ParseIntInRangeOptions {
  DEF_IF_TOO_SMALL = 0,
  MIN_IF_TOO_SMALL = 1,
  DEF_IF_TOO_LARGE = 0,
  MAX_IF_TOO_LARGE = 2,
};

// Parses s as an int in range [min, max] represented in base 10.
// If it cannot be parsed, returns defValue.
// If it is out of range, returns defValue, min or max, depending on options.
int parseIntInRange(const std::string& s, int min, int max, int defValue,
                    int options = DEF_IF_TOO_SMALL | DEF_IF_TOO_LARGE);

enum TrimOptions {
  TRIM_LEFT = 1,
  TRIM_RIGHT = 2,
  TRIM_BOTH = TRIM_LEFT | TRIM_RIGHT,
};

struct StringBorders {
  int64_t left;
  int64_t right;
};

// trimOptions is a combination of flags from TrimOptions.
StringBorders getTrimmedBorders(std::string_view s, int trimOptions);
// trimOptions is a combination of flags from TrimOptions.
std::string_view trim(std::string_view s, int trimOptions = TRIM_BOTH);
bool isSpace(std::string_view s);
std::string collapseSpace(std::string_view s);
std::string trimAndCollapseSpace(std::string_view s);

std::string toLowerCaseASCII(std::string_view s);

/* Returns true if c is in '0'-'9', 'A'-'F' or 'a'-'f'. */
inline bool isHexDigit(char c) {
  return string_internal::INT_OF_HEX_DIGIT[static_cast<unsigned char>(c)] != -1;
}

/* Converts '0'-'9' to 0-9, 'A'-'F' and 'a'-'f' to 10-15.
   Returns -1 if c is not a valid hexadecimal digit. */
inline int intOfHexDigit(char c) {
  return string_internal::INT_OF_HEX_DIGIT[static_cast<unsigned char>(c)];
}

class FieldGenerator {
public:
  using value_type = std::string_view;
  FieldGenerator(std::string_view str, char separator, bool ignoreLastFieldIfEmpty = false);
  bool next();
  std::string_view value() const { return m_value; }

private:
  std::string_view m_unconsumedPart;
  std::string_view m_value;
  char m_separator;
  bool m_atEnd = false;
};

class LineGenerator : public FieldGenerator {
public:
  LineGenerator(std::string_view str) : FieldGenerator(str, '\n', true) {}
};

using split = cbl::GeneratedRange<FieldGenerator>;
using splitLines = cbl::GeneratedRange<LineGenerator>;
std::vector<std::string_view> splitAsVector(std::string_view str, char separator, bool ignoreLastFieldIfEmpty = false);
std::vector<std::string_view> splitLinesAsVector(std::string_view str);

std::string replace(std::string_view text, std::string_view oldFragment, std::string_view newFragment);
int replaceInPlace(std::string& text, std::string_view oldFragment, std::string_view newFragment);

void encodeURIComponentCat(std::string_view str, std::string& buffer);
std::string encodeURIComponent(std::string_view str);
std::string decodeURIComponent(std::string_view str);

std::string shellEscape(std::string_view str);

template <class T>
std::string join(const T& begin, const T& end, std::string_view delimiter) {
  std::string result;
  T it = begin;
  if (it != end) {
    result += *it;
    for (++it; it != end; ++it) {
      result += delimiter;
      result += *it;
    }
  }
  return result;
}

template <class T>
inline std::string join(const T& items, std::string_view delimiter) {
  return join(items.begin(), items.end(), delimiter);
}

std::string unindent(std::string_view s);

}  // namespace cbl

#endif
