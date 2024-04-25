#include "json.h"
#include <string>
#include "cbl/json.h"

using std::string;

void JSONObject::add(const string& key, const JSONValue& value) {
  if (buffer.size() > 1) buffer += ',';
  json::quoteCat(key, buffer);
  buffer += ":";
  buffer += value.toString();
}

const string& JSONObject::toString() const {
  if (!finalized) buffer += "}";
  finalized = true;
  return buffer;
}

void JSONArray::add(const JSONValue& value) {
  if (buffer.size() > 1) buffer += ',';
  buffer += value.toString();
}

const string& JSONArray::toString() const {
  if (!finalized) buffer += "]";
  finalized = true;
  return buffer;
}

JSONValue::JSONValue(int value) {
  buffer += std::to_string(value);
}

JSONValue::JSONValue(const string& value) {
  json::quoteCat(value, buffer);
}

JSONValue::JSONValue(const char* value) {
  json::quoteCat(value, buffer);
}

JSONValue::JSONValue(const JSONObject& object) {
  buffer = object.toString();
}

JSONValue::JSONValue(const JSONArray& array) {
  buffer = array.toString();
}

const string& JSONValue::toString() const {
  return buffer;
}
