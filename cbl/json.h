// Parsing and serialization of data in JSON format.
//
// Parsing JSON:
//   json::Value value = json::parse(R"({"results": [1, 2, 3]})");
//   std::cout << "The first result is " << value["results"][0].numberAsInt() << "\n";
//
// Generating JSON:
//   json::Value value;
//   json::Value& results = value.getMutable("results");
//   results.addItem() = 1;
//   results.addItem() = 2;
//   results.addItem() = 3;
//   std::cout << value.toJSON() << "\n";
//
// There is currently no support for floating point values. The parser can process data containing floating point
// values, but there is no way to read them afterwards.
#ifndef CBL_JSON_H
#define CBL_JSON_H

#include <cstdint>
#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace json {

void quoteCat(std::string_view str, std::string& buffer);
std::string quote(std::string_view str);
std::string unquotePartial(std::string_view& s);

enum ValueType {
  VT_NULL,
  VT_BOOL,
  VT_NUMBER,
  VT_STRING,
  VT_OBJECT,
  VT_ARRAY,
};

// Ad-hoc variant type that can be parsed from and serialized to JSON.
class Value {
public:
  class ArrayAccessor {
  public:
    class iterator {
    public:
      explicit iterator(const Value* const* p = nullptr) : p(p) {}
      bool operator!=(const iterator& it) const { return p != it.p; }
      const Value& operator*() const { return **p; }
      iterator operator++() { return iterator(++p); }

    private:
      const Value* const* p;
    };

    explicit ArrayAccessor(const std::vector<Value*>* array) : m_array(array) {}
    bool empty() const { return m_array->empty(); }
    int size() const { return m_array->size(); }
    iterator begin() const { return iterator(m_array->empty() ? nullptr : &(*m_array)[0]); }
    iterator end() const { return iterator(m_array->empty() ? nullptr : &(*m_array)[0] + m_array->size()); }
    const Value& operator[](int index) const;

  private:
    const std::vector<Value*>* m_array;
  };
  class ObjectAccessor {
  public:
    explicit ObjectAccessor(const std::map<std::string, Value>* object) : m_object(object) {}
    bool empty() const { return m_object->empty(); }
    int size() const { return m_object->size(); }
    std::map<std::string, Value>::const_iterator begin() const { return m_object->begin(); }
    std::map<std::string, Value>::const_iterator end() const { return m_object->end(); }
    const Value& operator[](const std::string& key) const;
    const Value& firstValue() const;

  private:
    const std::map<std::string, Value>* m_object;
  };

  Value(){};
  explicit Value(const void*) = delete;  // No implicit pointer -> bool conversion.
  explicit Value(bool b) { setBoolean(b); }
  explicit Value(int number) { setNumber(number); }
  explicit Value(int64_t number) { setNumber(number); }
  explicit Value(std::string_view s) { setStr(s); }
  explicit Value(const char* s) : Value(std::string_view(s)) {}
  Value(const Value& otherValue) = delete;
  Value(Value&& otherValue);
  ~Value() { setNull(); }

  Value& operator=(const Value& otherValue) = delete;
  // "value = std::move(value)" is a valid noop.
  // "value = std::move(value.getMutable("some_key"))" is allowed.
  // "value.getMutable("some_key") = std::move(value)" is not allowed (it would create cycles).
  Value& operator=(Value&& otherValue);
  Value& operator=(const void*) = delete;
  Value& operator=(bool b) {
    setBoolean(b);
    return *this;
  }
  Value& operator=(int number) {
    setNumber(number);
    return *this;
  }
  Value& operator=(int64_t number) {
    setNumber(number);
    return *this;
  }
  Value& operator=(std::string_view s) {
    setStr(s);
    return *this;
  }
  Value& operator=(const char* s) { return *this = std::string_view(s); }
  Value copy() const;
  void swap(Value& otherValue);

  ValueType getType() const { return m_type; }
  bool isNull() const { return m_type == VT_NULL; }
  bool isBoolean() const { return m_type == VT_BOOL; }
  bool isNumber() const { return m_type == VT_NUMBER; }
  bool isString() const { return m_type == VT_STRING; }
  bool isObject() const { return m_type == VT_OBJECT; }
  bool isArray() const { return m_type == VT_ARRAY; }

  void setNull() { setType(VT_NULL); }
  bool boolean() const;
  void setBoolean(bool b);
  int numberAsInt() const;
  int64_t numberAsInt64() const;
  void setNumber(int number);
  void setNumber(int64_t number);
  const std::string& str() const;
  void setStr(std::string_view s);
  ArrayAccessor array() const { return ArrayAccessor(m_type == VT_ARRAY ? m_data.array : &EMPTY_ARRAY); }
  void setToEmptyArray() { resize(0); }
  ObjectAccessor object() const { return ObjectAccessor(m_type == VT_OBJECT ? m_data.object : &EMPTY_OBJECT); }
  void setToEmptyObject();

  bool operator==(const Value& otherValue) const;
  bool operator!=(const Value& otherValue) const { return !(*this == otherValue); }

  bool has(const std::string& key) const;
  Value& getMutable(const std::string& key);
  const Value& operator[](const std::string& key) const { return object()[key]; }
  void erase(const std::string& key);
  typedef std::map<std::string, Value>::iterator iterator;
  typedef std::map<std::string, Value>::const_iterator const_iterator;
  iterator begin();
  const_iterator begin() const;
  iterator end();
  const_iterator end() const;

  Value& getMutable(int index);
  const Value& operator[](int index) const { return array()[index]; }
  void resize(int newSize);
  Value& addItem();

  void toJSONCat(std::string& buffer) const;
  std::string toJSON() const {
    std::string buffer;
    toJSONCat(buffer);
    return buffer;
  }

private:
  void reallocArray(int newSize);
  void setType(ValueType newType);

  static const std::vector<Value*> EMPTY_ARRAY;
  static const std::map<std::string, Value> EMPTY_OBJECT;

  ValueType m_type = VT_NULL;
  union {
    void* rawPointer;
    const std::string* boolData;
    std::string* str;
    // TODO: Clean up places that depend on pointer stability when adding elements and change to std::vector<Value>*.
    std::vector<Value*>* array;
    std::map<std::string, Value>* object;
  } m_data;

  friend class ValueParser;
};

std::ostream& operator<<(std::ostream& os, const Value& value);

Value parse(std::string_view s);
Value parsePartial(std::string_view& s);

}  // namespace json

#endif
