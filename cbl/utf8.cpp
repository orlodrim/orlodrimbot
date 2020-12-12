#include "utf8.h"
#include <algorithm>
#include <string>
#include <string_view>

using std::string;
using std::string_view;

namespace cbl {
namespace utf8 {

int consumeChar(string_view& str) {
  if (str.empty()) {
    return -1;
  }
  size_t size = str.size();
  int character = -1;
  int characterSize = 1;
  int byte1 = static_cast<unsigned char>(str[0]);
  if (byte1 < 0x80) {
    character = byte1;
  } else if (byte1 >= 0xC0 && size >= 2) {
    int byte2 = static_cast<unsigned char>(str[1]);
    if (byte2 >= 0x80 && byte2 < 0xC0) {
      if (byte1 < 0xE0) {
        int c = ((byte1 & 0x1F) << 6) + (byte2 & 0x3F);
        if (c >= 0x80) {
          character = c;
          characterSize = 2;
        }
      } else if (size >= 3) {
        int byte3 = static_cast<unsigned char>(str[2]);
        if (byte3 >= 0x80 && byte3 < 0xC0) {
          if (byte1 < 0xF0) {
            int c = ((byte1 & 0xF) << 12) + ((byte2 & 0x3F) << 6) + (byte3 & 0x3F);
            if (c >= 0x800) {
              character = c;
              characterSize = 3;
            }
          } else if (size >= 4 && byte1 < 0xF8) {
            int byte4 = static_cast<unsigned char>(str[3]);
            if (byte4 >= 0x80 && byte4 < 0xC0) {
              int c = ((byte1 & 0x7) << 18) + ((byte2 & 0x3F) << 12) + ((byte3 & 0x3F) << 6) + (byte4 & 0x3F);
              if (c >= 0x10000) {
                character = c;
                characterSize = 4;
              }
            }
          }
        }
      }
    }
  }
  str.remove_prefix(characterSize);
  return character;
}

int consumeCharFromEnd(string_view& str) {
  if (str.empty()) {
    return -1;
  }
  const char* end = str.data() + str.size();
  int characterSize = 1;
  int maxSize = str.size() < 4 ? str.size() : 4;
  for (; characterSize < maxSize && (*(end - characterSize) & 0xC0) == 0x80; characterSize++) {
  }
  string_view encodedChar(end - characterSize, characterSize);
  int character = consumeChar(encodedChar);
  if (!encodedChar.empty()) {
    character = -1;
    characterSize = 1;
  }
  str.remove_suffix(characterSize);
  return character;
}

string_view encode(int character, EncodeBuffer& buffer) {
  int size = 0;
  if (character >= 0) {
    if (character < 0x80) {
      buffer.bytes[0] = character;
      size = 1;
    } else if (character < 0x800) {
      buffer.bytes[0] = static_cast<unsigned char>(0xC0 | (character >> 6));
      buffer.bytes[1] = static_cast<unsigned char>(0x80 | (character & 0x3F));
      buffer.bytes[2] = 0;
      size = 2;
    } else if (character < 0x10000) {
      buffer.bytes[0] = static_cast<unsigned char>(0xE0 | (character >> 12));
      buffer.bytes[1] = static_cast<unsigned char>(0x80 | ((character >> 6) & 0x3F));
      buffer.bytes[2] = static_cast<unsigned char>(0x80 | (character & 0x3F));
      buffer.bytes[3] = 0;
      size = 3;
    } else if (character < 0x200000) {
      buffer.bytes[0] = static_cast<unsigned char>(0xF0 | (character >> 18));
      buffer.bytes[1] = static_cast<unsigned char>(0x80 | ((character >> 12) & 0x3F));
      buffer.bytes[2] = static_cast<unsigned char>(0x80 | ((character >> 6) & 0x3F));
      buffer.bytes[3] = static_cast<unsigned char>(0x80 | (character & 0x3F));
      size = 4;
    }
  }
  return string_view(&buffer.bytes[0], size);
}

int len(string_view s) {
  for (int length = 0;; length++) {
    int c = consumeChar(s);
    if (c == -1) {
      return length;
    }
  }
}

static int getSizeOfFirstNChars(string_view str, int n) {
  string_view consumedStr = str;
  for (; n > 0 && !consumedStr.empty(); n--) {
    consumeChar(consumedStr);
  }
  return str.size() - consumedStr.size();
}

static int getSizeOfLastNChars(string_view str, int n) {
  string_view consumedStr = str;
  for (; n > 0 && !consumedStr.empty(); n--) {
    consumeCharFromEnd(consumedStr);
  }
  return str.size() - consumedStr.size();
}

static int getPositionFromSignedIndex(string_view s, int index) {
  return index >= 0 ? getSizeOfFirstNChars(s, index) : s.size() - getSizeOfLastNChars(s, -index);
}

string_view substring(string_view str, int start, int end) {
  int startByte = getPositionFromSignedIndex(str, start);
  int size;
  if (start >= 0 && end >= 0) {
    size = getSizeOfFirstNChars(str.substr(startByte), end - start);  // = startByte if end < start.
  } else {
    size = getPositionFromSignedIndex(str, end) - startByte;  // May be negative.
  }
  return str.substr(startByte, std::max(size, 0));
}

string limitStringLength(string_view str, int maxLength) {
  string result;
  if (maxLength > 0) {
    string_view truncatedStr = substring(str, 0, maxLength);
    if (truncatedStr.size() < str.size()) {
      truncatedStr.remove_suffix(getSizeOfLastNChars(truncatedStr, 3));
      result.reserve(truncatedStr.size() + 3);
      result += truncatedStr;
      result += maxLength >= 3 ? "..." : string(maxLength, '.');
    } else {
      result = str;
    }
  }
  return result;
}

}  // namespace utf8
}  // namespace cbl
