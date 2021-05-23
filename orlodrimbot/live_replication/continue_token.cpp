#include "continue_token.h"
#include <cstdint>
#include <string>
#include <string_view>
#include "cbl/error.h"
#include "cbl/string.h"

using std::string;
using std::string_view;

namespace live_replication {

int64_t parseContinueToken(string_view token, string_view expectedType) {
  size_t pipePosition = token.find('|');
  if (pipePosition == string_view::npos) {
    throw cbl::ParseError("Invalid continue token: '" + string(token) + "'");
  }
  string_view type = token.substr(0, pipePosition);
  if (type != expectedType) {
    throw cbl::ParseError("Invalid continue token (wrong type): '" + string(token) + "'");
  }
  return cbl::parseInt64(cbl::legacyStringConv(token.substr(pipePosition + 1)));
}

string buildContinueToken(string_view type, int64_t data) {
  string dataStr = std::to_string(data);
  string result;
  result.reserve(type.size() + dataStr.size() + 1);
  result += type;
  result += '|';
  result += dataStr;
  return result;
}

}  // namespace live_replication
