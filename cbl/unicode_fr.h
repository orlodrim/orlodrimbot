// Unicode functions for French.
// They should work reasonably in English as well, but supporting all languages would require a dedicated library.
#ifndef CBL_UNICODE_FR_H
#define CBL_UNICODE_FR_H

#include <string>
#include <string_view>
#include "utf8.h"

namespace unicode_fr {

// The returned string_view may point to data stored in buffer or to some statically allocated data.
// Do not assume that the output contains a single char.
std::string_view charToLowerCase(int character, cbl::utf8::EncodeBuffer& buffer);
std::string_view charToUpperCase(int character, cbl::utf8::EncodeBuffer& buffer);
std::string_view charToTitleCase(int character, cbl::utf8::EncodeBuffer& buffer);

std::string toLowerCase(std::string_view text);
std::string toUpperCase(std::string_view text);
// Puts the first letter in title case.
std::string capitalize(std::string_view text);

}  // namespace unicode_fr

#endif
