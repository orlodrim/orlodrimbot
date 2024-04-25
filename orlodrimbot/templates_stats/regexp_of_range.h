#ifndef REGEXP_OF_RANGE_H
#define REGEXP_OF_RANGE_H

#include <string>
#include <string_view>

// Requirements: min <= max, min and max do not start with '0' unless they are equal to "0". An empty value for max
// means +infinity.
std::string buildRegExpForRange(std::string_view min, std::string_view max);

#endif
