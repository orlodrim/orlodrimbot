#include "algorithm.h"
#include <memory>
#include <string_view>
#include <utility>

using std::string_view;
using std::unique_ptr;

namespace talk_page_archiver {

void Algorithms::add(unique_ptr<Algorithm> algorithm) {
  algorithm->setRank(m_algorithmsByName.size());
  string_view name = algorithm->name();
  m_algorithmsByName.insert({name, std::move(algorithm)});
}

const Algorithm& Algorithms::find(string_view name) const {
  return *m_algorithmsByName.at(name);
}

}  // namespace talk_page_archiver
