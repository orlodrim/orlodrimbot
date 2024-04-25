#ifndef JSON_H
#define JSON_H

#include <string>

class JSONValue;

class JSONObject {
public:
  JSONObject() : buffer("{"), finalized(false) {}
  void add(const std::string& key, const JSONValue& value);
  const std::string& toString() const;

private:
  mutable std::string buffer;
  mutable bool finalized;
};

class JSONArray {
public:
  JSONArray() : buffer("["), finalized(false) {}
  void add(const JSONValue& value);
  const std::string& toString() const;

private:
  mutable std::string buffer;
  mutable bool finalized;
};

class JSONValue {
public:
  JSONValue(int value);
  JSONValue(const char* value);
  JSONValue(const std::string& value);
  JSONValue(const JSONObject& object);
  JSONValue(const JSONArray& array);
  const std::string& toString() const;

private:
  std::string buffer;
};

#endif
