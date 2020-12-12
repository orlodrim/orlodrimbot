#ifndef CBL_UTF8_H
#define CBL_UTF8_H

#include <climits>
#include <string>
#include <string_view>

namespace cbl {
namespace utf8 {

// Maximum number of bytes in a UTF-8 character.
constexpr int MAX_UTF8_CHAR_SIZE = 4;

// Consumes one UTF-8 encoded char from the left of `str` and returns it.
// If `str` is empty, returns -1 and leaves it unchanged.
// If `str` does not start with a valid UTF-8 character, consumes one byte of it and returns -1.
// '\0' is considered as a valid character and does not have any special treatment.
int consumeChar(std::string_view& str);

// Same as consumeChar, but consumes from the right of `str`.
int consumeCharFromEnd(std::string_view& str);

// Buffer storing the internal result of encode. Should not be accessed directly.
struct EncodeBuffer {
  char bytes[MAX_UTF8_CHAR_SIZE];
};

// Returns a string_view of `character` encoded in UTF-8. If `character` is out of the range of valid UTF-8 characteres,
// returns an empty string_view. `buffer` is used to store the data of the returned string_view.
std::string_view encode(int character, EncodeBuffer& buffer);

// Returns the number of UTF-8 characters in s. Any byte that is not part of a valid UTF-8 character counts as 1.
int len(std::string_view s);

// Returns characters [start, end) from str. Negative values for start and end are counted from the end of the string,
// e.g. substring("abcdef", -3, -1) = "de".
std::string_view substring(std::string_view str, int start, int end = INT_MAX);

// Returns a string containing `str`, or the first characters of `str' + "..." if it is longer than `maxLength` UTF-8
// characters (so that the result contains at most maxLength characters).
// WARNING: This only makes sense in some languages, and truncations may occasionally produce curse words or other
// inappropriate sentences.
std::string limitStringLength(std::string_view str, int maxLength);

}  // namespace utf8
}  // namespace cbl

#endif
