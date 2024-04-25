#include "regexp_of_range.h"
#include <string>
#include <string_view>
#include "cbl/log.h"
#include "cbl/string.h"

using std::string;
using std::string_view;

static void appendRegExpForRange(string_view min, string_view max, string& buffer) {
  CBL_ASSERT(!min.empty());
  int digitsInMin = min.size();
  if (max.empty()) {
    if (min == "1") {
      buffer += "[1-9]\\d*|";
    } else {
      appendRegExpForRange(min, string(digitsInMin, '9'), buffer);
      cbl::append(buffer, "[1-9]\\d{", std::to_string(digitsInMin), ",}|");
    }
    return;
  }
  int digitsInMax = max.size();
  if (digitsInMin < digitsInMax) {
    appendRegExpForRange(min, string(digitsInMin, '9'), buffer);
    if (digitsInMin < digitsInMax - 1) {
      cbl::append(buffer, "[1-9]\\d{", std::to_string(digitsInMin), ",", std::to_string(digitsInMax - 2), "}|");
    }
    appendRegExpForRange("1" + string(digitsInMax - 1, '0'), max, buffer);
    return;
  }
  CBL_ASSERT(digitsInMin == digitsInMax && min <= max) << "min=" << min << " max=" << max;
  bool commonPrefix = false;
  while (!min.empty() && min[0] == max[0]) {
    buffer += min[0];
    min.remove_prefix(1);
    max.remove_prefix(1);
    commonPrefix = true;
  }
  int differentDigits = min.size();
  if (differentDigits > 0) {
    int tailSize = differentDigits - 1;
    bool lowerBoundIsTrivial = min.substr(1) == string(tailSize, '0');
    bool upperBoundIsTrivial = max.substr(1) == string(tailSize, '9');
    if (commonPrefix) {
      buffer += '(';
    }
    if (!lowerBoundIsTrivial) {
      appendRegExpForRange(min, string(1, min[0]) + string(tailSize, '9'), buffer);
    }
    char rangeStart = min[0] + (lowerBoundIsTrivial ? 0 : 1);
    char rangeEnd = max[0] - (upperBoundIsTrivial ? 0 : 1);
    if (rangeStart <= rangeEnd) {
      if (rangeStart == rangeEnd) {
        buffer += rangeStart;
      } else {
        buffer += '[';
        buffer += rangeStart;
        buffer += '-';
        buffer += rangeEnd;
        buffer += ']';
      }
      if (tailSize > 1) {
        cbl::append(buffer, "\\d{", std::to_string(tailSize), "}");
      } else if (tailSize == 1) {
        buffer += "\\d";
      }
      buffer += '|';
    }
    if (!upperBoundIsTrivial) {
      appendRegExpForRange(string(1, max[0]) + string(tailSize, '0'), max, buffer);
    }
    CBL_ASSERT(cbl::endsWith(buffer, "|"));
    buffer.pop_back();
    if (commonPrefix) {
      buffer += ')';
    }
  }
  buffer += '|';
}

string buildRegExpForRange(string_view min, string_view max) {
  string buffer;
  appendRegExpForRange(min, max, buffer);
  CBL_ASSERT(cbl::endsWith(buffer, "|"));
  buffer.pop_back();
  return buffer;
}
