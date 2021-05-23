#include "json.h"
#include <ctype.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "error.h"
#include "utf8.h"

using cbl::ParseError;
using std::map;
using std::string;
using std::string_view;
using std::vector;

namespace json {

static const Value NULL_VALUE;
static const string EMPTY_STRING;
const vector<Value*> Value::EMPTY_ARRAY;
const map<string, Value> Value::EMPTY_OBJECT;
map<string, Value> MUTABLE_EMPTY_OBJECT;  // Never actually mutated (only used for begin() and end()).
constexpr char HEX_DIGITS[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

static void skipSpace(string_view& s) {
  for (; !s.empty() && isspace(static_cast<unsigned char>(s[0])); s.remove_prefix(1)) {
  }
}

static bool parseChar(string_view& s, char c) {
  if (!s.empty() && s[0] == c) {
    s.remove_prefix(1);
    return true;
  } else {
    return false;
  }
}

static int parseStringUEscape(string_view& s) {
  if (s.size() < 4) {
    throw ParseError("Invalid string: incomplete UTF-8 character '\\u" + string(s) + "' at the end");
  }
  char charNumBuffer[5];
  memcpy(charNumBuffer, s.data(), 4);
  charNumBuffer[4] = '\0';
  char* endPtr = nullptr;
  int c = strtol(charNumBuffer, &endPtr, 16);
  if (endPtr != charNumBuffer + 4) {
    throw ParseError("Invalid string: invalid UTF-8 character '\\u" + string(charNumBuffer) + "'");
  }
  s.remove_prefix(4);
  return c;
}

void quoteCat(string_view str, string& buffer) {
  buffer += '"';
  for (unsigned char c : str) {
    if (c < 0x20) {
      if (c == '\n') {
        buffer += "\\n";
      } else if (c == '\t') {
        buffer += "\\t";
      } else if (c == '\r') {
        buffer += "\\r";
      } else {
        char charBuffer[7] = {'\\', 'u', '0', '0', HEX_DIGITS[c >> 4], HEX_DIGITS[c & 0xF], 0};
        buffer += charBuffer;
      }
    } else if (c == '\\') {
      buffer += "\\\\";
    } else if (c == '"') {
      buffer += "\\\"";
    } else {
      buffer += static_cast<char>(c);
    }
  }
  buffer += '"';
}

string quote(string_view str) {
  string result;
  quoteCat(str, result);
  return result;
}

string unquotePartial(string_view& s) {
  if (!parseChar(s, '"')) {
    throw ParseError("Invalid string: missing left quotes");
  }
  string data;
  while (true) {
    size_t position = 0;
    for (char c : s) {
      if (c == '"' || c == '\\' || c == '\0') break;
      position++;
    }
    if (position == s.size()) {
      throw ParseError("Invalid string: missing closing quotes");
    }
    data += s.substr(0, position);
    char charFound = s[position];
    s.remove_prefix(position + 1);
    if (charFound == '"') break;
    if (charFound == '\0') {
      throw ParseError("Invalid string: contains raw nul char");
    }
    // charFound is a backslash.
    if (s.empty()) {
      throw ParseError("Invalid string: missing escaped char after '\\' and closing quotes");
    }
    char escapedChar = s[0];
    switch (escapedChar) {
      case '"':
      case '\\':
      case '/':
        data += escapedChar;
        break;
      case 'b':
        data += '\b';
        break;
      case 'f':
        data += '\f';
        break;
      case 'n':
        data += '\n';
        break;
      case 'r':
        data += '\r';
        break;
      case 't':
        data += '\t';
        break;
      case 'u': {
        s.remove_prefix(1);
        int c = parseStringUEscape(s);
        // entre D800 et DBFF, premier caractère UTF-16
        // entre DC00 et DFFF, second caractère UTF-16
        // 110110ww -> 11111100 -> FC00
        if (c >= 0xD800 && c <= 0xDFFF) {
          if (c >= 0xDC00) {
            throw ParseError("Invalid string: invalid UTF-16 character (bad range 0xDC00-0xDFFF)");
          } else if (s.size() < 6 || s[0] != '\\' || s[1] != 'u') {
            throw ParseError("Invalid string: partial UTF-16 character");
          }
          s.remove_prefix(2);
          int cHigh = (c & 0x3FF) + 0x40;
          int cLow = parseStringUEscape(s) & 0x3FF;
          c = (cHigh << 10) + cLow;
        }
        cbl::utf8::EncodeBuffer encodeBuffer;
        data += cbl::utf8::encode(c, encodeBuffer);
        continue;  // Do not call s.remove_prefix(1).
      }
      default:
        throw ParseError("Invalid escape sequence in string: '\\" + string(1, escapedChar) + "'");
    }
    s.remove_prefix(1);
  }
  return data;
}

static void addIndentedLine(string& buffer, int depth) {
  buffer += '\n';
  buffer.append(depth * 2, ' ');
}

const Value& Value::ArrayAccessor::operator[](int index) const {
  if (index < 0) {
    throw std::out_of_range("json::ArrayAccessor::operator[] called with a negative index");
  }
  return index < size() ? *(*m_array)[index] : NULL_VALUE;
}

const Value& Value::ObjectAccessor::operator[](const string& key) const {
  map<string, Value>::const_iterator it = m_object->find(key);
  return it == m_object->end() ? NULL_VALUE : it->second;
}

const Value& Value::ObjectAccessor::firstValue() const {
  return m_object->empty() ? NULL_VALUE : m_object->begin()->second;
}

Value::Value(Value&& otherValue) {
  m_type = otherValue.m_type;
  m_data.rawPointer = otherValue.m_data.rawPointer;
  otherValue.m_type = VT_NULL;
}

Value& Value::operator=(Value&& otherValue) {
  if (this != &otherValue) {
    // Retain the current value of *this until the assignment is done, in case otherValue is a part of *this
    // (see MoveAssignmentChild test) and then automatically free it.
    Value oldThis(std::move(*this));
    m_type = otherValue.m_type;
    m_data.rawPointer = otherValue.m_data.rawPointer;
    otherValue.m_type = VT_NULL;
  }
  return *this;
}

void Value::swap(Value& otherValue) {
  std::swap(m_type, otherValue.m_type);
  std::swap(m_data.rawPointer, otherValue.m_data.rawPointer);
}

Value Value::copy() const {
  Value newValue;
  newValue.setType(m_type);
  switch (m_type) {
    case VT_NULL:
      break;
    case VT_BOOL:
      newValue.m_data.boolData = m_data.boolData;
      break;
    case VT_NUMBER:
    case VT_STRING:
      *newValue.m_data.str = *m_data.str;
      break;
    case VT_OBJECT:
      for (const auto& [key, value] : *m_data.object) {
        (*newValue.m_data.object)[key] = value.copy();
      }
      break;
    case VT_ARRAY:
      newValue.m_data.array->reserve(m_data.array->size());
      for (const Value* vItem : *m_data.array) {
        newValue.m_data.array->push_back(new Value(vItem->copy()));
      }
      break;
  }
  return newValue;
}

bool Value::boolean() const {
  return m_type == VT_BOOL && m_data.boolData;
}

void Value::setBoolean(bool b) {
  setType(VT_BOOL);
  m_data.boolData = b ? &EMPTY_STRING : nullptr;
}

int Value::numberAsInt() const {
  return m_type == VT_NUMBER ? atoi(m_data.str->c_str()) : 0;
}

int64_t Value::numberAsInt64() const {
  return m_type == VT_NUMBER ? atoll(m_data.str->c_str()) : 0;
}

void Value::setNumber(int number) {
  setType(VT_NUMBER);
  *m_data.str = std::to_string(number);
}

void Value::setNumber(int64_t number) {
  setType(VT_NUMBER);
  *m_data.str = std::to_string(number);
}

const string& Value::str() const {
  return m_type == VT_STRING ? *m_data.str : EMPTY_STRING;
}

void Value::setStr(string_view s) {
  setType(VT_STRING);
  // Builds a string explicitly and then move it, in case s.data == m_data.str->c_str() (see test AssignOwnString).
  *m_data.str = string(s);
}

bool Value::operator==(const Value& otherValue) const {
  if (m_type != otherValue.m_type) {
    return false;
  }
  switch (m_type) {
    case VT_NULL:
      return true;  // null == null
    case VT_BOOL:
      return boolean() == otherValue.boolean();
    case VT_NUMBER:
      return numberAsInt64() == otherValue.numberAsInt64();
    case VT_STRING:
      return *m_data.str == *otherValue.m_data.str;
    case VT_OBJECT:
      return *m_data.object == *otherValue.m_data.object;
    case VT_ARRAY:
      // The array contain pointers, so we cannot check if the arrays are equal
      if (m_data.array->size() != otherValue.m_data.array->size()) return false;
      for (int i = m_data.array->size() - 1; i >= 0; i--) {
        if (*(*m_data.array)[i] != *(*otherValue.m_data.array)[i]) return false;
      }
      return true;
  }
  // Should not happen.
  return true;
}

bool Value::has(const string& key) const {
  return m_type == VT_OBJECT ? m_data.object->find(key) != m_data.object->end() : false;
}

Value& Value::getMutable(const string& key) {
  setType(VT_OBJECT);
  return (*m_data.object)[key];
};

void Value::erase(const string& key) {
  if (m_type == VT_OBJECT) {
    m_data.object->erase(key);
  }
}

Value::iterator Value::begin() {
  return m_type == VT_OBJECT ? m_data.object->begin() : MUTABLE_EMPTY_OBJECT.begin();
}

Value::const_iterator Value::begin() const {
  return m_type == VT_OBJECT ? m_data.object->begin() : EMPTY_OBJECT.begin();
}

Value::iterator Value::end() {
  return m_type == VT_OBJECT ? m_data.object->end() : MUTABLE_EMPTY_OBJECT.end();
}

Value::const_iterator Value::end() const {
  return m_type == VT_OBJECT ? m_data.object->end() : EMPTY_OBJECT.end();
}

Value& Value::getMutable(int index) {
  if (index < 0) {
    throw std::out_of_range("json::Value::getMutable called with a negative index");
  }
  setType(VT_ARRAY);
  if (index >= static_cast<int>(m_data.array->size())) {
    reallocArray(index + 1);
  }
  return *(*m_data.array)[index];
}

void Value::reallocArray(int newSize) {
  if (newSize < 0) {
    throw std::invalid_argument("json::Value::reallocArray called with a negative size");
  }
  int oldSize = m_data.array->size();
  for (int i = newSize; i < oldSize; i++) {
    delete (*m_data.array)[i];
  }
  m_data.array->resize(newSize);
  for (int i = oldSize; i < newSize; i++) {
    (*m_data.array)[i] = new Value;
  }
}

void Value::resize(int newSize) {
  setType(VT_ARRAY);
  reallocArray(newSize);
}

Value& Value::addItem() {
  setType(VT_ARRAY);
  int n = m_data.array->size();
  reallocArray(n + 1);
  return *(*m_data.array)[n];
}

void Value::setToEmptyObject() {
  setType(VT_OBJECT);
  m_data.object->clear();
}

void Value::setType(ValueType newType) {
  if (m_type != newType) {
    if (m_type == VT_NUMBER || m_type == VT_STRING) {
      delete m_data.str;
    } else if (m_type == VT_OBJECT) {
      delete m_data.object;
    } else if (m_type == VT_ARRAY) {
      reallocArray(0);
      delete m_data.array;
    }
    if (newType == VT_NUMBER || newType == VT_STRING) {
      m_data.str = new string;
    } else if (newType == VT_OBJECT) {
      m_data.object = new map<string, Value>;
    } else if (newType == VT_ARRAY) {
      m_data.array = new vector<Value*>;
    }
    m_type = newType;
  }
}

void Value::toJSONCat(string& buffer, Style style, int depth) const {
  switch (m_type) {
    case VT_NULL:
      buffer += "null";
      break;
    case VT_BOOL:
      buffer += m_data.boolData ? "true" : "false";
      break;
    case VT_NUMBER:
      buffer += *m_data.str;
      break;
    case VT_STRING:
      quoteCat(*m_data.str, buffer);
      break;
    case VT_OBJECT: {
      buffer += '{';
      bool notFirst = false;
      for (const auto& [key, value] : *m_data.object) {
        if (notFirst) {
          buffer += ',';
        }
        if (style == INDENTED) {
          addIndentedLine(buffer, depth + 1);
        }
        notFirst = true;
        quoteCat(key, buffer);
        buffer += style == INDENTED ? ": " : ":";
        value.toJSONCat(buffer, style, depth + 1);
      }
      if (notFirst && style == INDENTED) {
        addIndentedLine(buffer, depth);
      }
      buffer += '}';
      break;
    }
    case VT_ARRAY: {
      buffer += '[';
      bool notFirst = false;
      for (const Value* value : *m_data.array) {
        if (notFirst) {
          buffer += ',';
        }
        if (style == INDENTED) {
          addIndentedLine(buffer, depth + 1);
        }
        notFirst = true;
        value->toJSONCat(buffer, style, depth + 1);
      }
      if (notFirst && style == INDENTED) {
        addIndentedLine(buffer, depth);
      }
      buffer += ']';
      break;
    }
  }
}

class ValueParser {
public:
  static Value parseValue(string_view& s);

private:
  static Value parseAnyType(string_view& s);
  static Value parseKeyword(string_view& s);
  static Value parseNumber(string_view& s);
  static Value parseString(string_view& s);
  static Value parseObject(string_view& s);
  static Value parseArray(string_view& s);
};

static string addErrorPosition(const char* errorMessage, string_view originalString, string_view currentString) {
  size_t position = currentString.data() - originalString.data();
  int line = 1, column = 0;
  for (size_t i = 0; i < position; i++) {
    column++;
    if (originalString[i] == '\n') {
      line++;
      column = 0;
    }
  }
  size_t endOfLine = originalString.find('\n', position);
  if (endOfLine == string_view::npos) {
    endOfLine = originalString.size();
  }
  string_view lineBeforePosition = originalString.substr(position - column, column);
  string_view lineAfterPosition = originalString.substr(position, endOfLine - position);
  string_view snippetBeforePosition = cbl::utf8::substring(lineBeforePosition, -40);
  string_view snippetAfterPosition = cbl::utf8::substring(lineAfterPosition, 0, 40);
  string message = errorMessage;
  message += ": '";
  if (snippetBeforePosition.size() < lineBeforePosition.size()) {
    message += "...";
  }
  message += snippetBeforePosition;
  message += "<error>";
  message += snippetAfterPosition;
  if (snippetAfterPosition.size() < lineAfterPosition.size()) {
    message += "...";
  }
  message += "' (line ";
  message += std::to_string(line);
  message += ", column ";
  message += std::to_string(column);
  message += ')';
  return message;
}

Value ValueParser::parseValue(string_view& s) {
  string_view originalString = s;
  try {
    return parseAnyType(s);
  } catch (const cbl::ParseError& error) {
    throw cbl::ParseError(addErrorPosition(error.what(), originalString, s));
  }
}

Value ValueParser::parseAnyType(string_view& s) {
  skipSpace(s);
  if (s.empty()) {
    throw ParseError("Expected value but found end of string");
  }
  char firstChar = s[0];
  if (firstChar == '{') {
    return parseObject(s);
  } else if (firstChar == '[') {
    return parseArray(s);
  } else if (firstChar == '"') {
    return parseString(s);
  } else if ((firstChar >= '0' && firstChar <= '9') || firstChar == '-') {
    return parseNumber(s);
  } else if (firstChar >= 'a' && firstChar <= 'z') {
    return parseKeyword(s);
  } else {
    throw ParseError("Unexpected character at the beginning of a value: '" + string(1, firstChar) + "'");
  }
}

Value ValueParser::parseKeyword(string_view& s) {
  size_t keywordSize = 0;
  for (; keywordSize < s.size() && s[keywordSize] >= 'a' && s[keywordSize] <= 'z'; keywordSize++) {
  }
  string_view keyword = s.substr(0, keywordSize);
  Value value;
  if (keyword == "null") {
    // Nothing to do.
  } else if (keyword == "true") {
    value.setType(VT_BOOL);
    value.m_data.boolData = &EMPTY_STRING;
  } else if (keyword == "false") {
    value.setType(VT_BOOL);
    value.m_data.boolData = nullptr;
  } else {
    throw ParseError("Invalid keyword '" + string(keyword) + "'");
  }
  s.remove_prefix(keywordSize);
  return value;
}

Value ValueParser::parseNumber(string_view& s) {
  size_t numberSize = 0;
  int numDots = 0;
  for (; numberSize < s.size(); numberSize++) {
    char c = s[numberSize];
    if (c == '.') {
      numDots++;
    } else if (!(c >= '0' && c <= '9') && c != '-' && c != '+' && c != 'e' && c != 'E') {
      break;
    }
  }
  // TODO: Validate the format. For now, dot counting is just a proof-on-concept that something can be rejected.
  if (numberSize == 0 || numDots > 1) {
    throw ParseError("Invalid number");
  }
  Value value;
  value.setType(VT_NUMBER);
  *value.m_data.str = s.substr(0, numberSize);
  s.remove_prefix(numberSize);
  return value;
}

Value ValueParser::parseString(string_view& s) {
  Value value;
  value.setType(VT_STRING);
  *value.m_data.str = unquotePartial(s);
  return value;
}

Value ValueParser::parseObject(string_view& s) {
  if (!parseChar(s, '{')) {
    throw ParseError("Expected '{' at the beginning of object");
  }
  skipSpace(s);
  Value value;
  value.setType(VT_OBJECT);
  if (!parseChar(s, '}')) {
    if (s.empty() || s[0] != '"') {  // This test improves error messages but is redundant otherwise.
      throw ParseError("Invalid object: expected string key or '}' after '{'");
    }
    while (true) {
      string key = unquotePartial(s);
      skipSpace(s);
      if (!parseChar(s, ':')) {
        throw ParseError("Invalid object: missing ':' after key");
      }
      (*value.m_data.object)[key] = parseAnyType(s);
      skipSpace(s);
      if (parseChar(s, '}')) break;
      if (!parseChar(s, ',')) {
        throw ParseError("Invalid object: missing ',' or '}' after value");
      }
      skipSpace(s);
      if (s.empty() || s[0] != '"') {  // This test improves error messages but is redundant otherwise.
        if (!s.empty() && s[0] == '}') {
          throw ParseError("Invalid object: trailing commas are not allowed before '}'");
        }
        throw ParseError("Invalid object: expected string key after ','");
      }
    }
  }
  return value;
}

Value ValueParser::parseArray(string_view& s) {
  if (!parseChar(s, '[')) {
    throw ParseError("Expected '[' at the beginning of array");
  }
  skipSpace(s);
  Value value;
  value.setType(VT_ARRAY);
  if (!parseChar(s, ']')) {
    while (true) {
      value.m_data.array->push_back(new Value(parseAnyType(s)));
      skipSpace(s);
      if (parseChar(s, ']')) break;
      if (!parseChar(s, ',')) {
        throw ParseError("Invalid array: expected ',' or ']' after value");
      }
    }
  }
  return value;
}

Value parsePartial(string_view& s) {
  return ValueParser::parseValue(s);
}

Value parse(string_view s) {
  Value value = ValueParser::parseValue(s);
  skipSpace(s);
  if (!s.empty()) {
    throw ParseError("Unexpected content after the end of the JSON string");
  }
  return value;
}

std::ostream& operator<<(std::ostream& os, const Value& value) {
  return os << value.toJSON();
}

}  // namespace json
