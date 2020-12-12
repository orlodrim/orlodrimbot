#ifndef CBL_GENERATED_RANGE_H
#define CBL_GENERATED_RANGE_H

#include <utility>

namespace cbl {

template <class Generator>
class GeneratorIterator {
public:
  GeneratorIterator() = default;
  explicit GeneratorIterator(Generator* generator) : m_generator(generator) { ++*this; }
  bool operator!=(GeneratorIterator it) { return m_generator != it.m_generator; }
  typename Generator::value_type operator*() { return m_generator->value(); }
  GeneratorIterator& operator++() {
    if (!m_generator->next()) {
      m_generator = nullptr;
    }
    return *this;
  }

private:
  Generator* m_generator = nullptr;
};

template <class Generator>
class GeneratedRange {
public:
  using iterator = GeneratorIterator<Generator>;
  template <typename... Args>
  explicit GeneratedRange(Args&&... args) : m_generator(std::forward<Args>(args)...) {}
  iterator begin() { return iterator(&m_generator); }
  iterator end() { return iterator(); }

private:
  Generator m_generator;
};

}  // namespace cbl

#endif
