#ifndef CBL_CONTAINERS_HELPERS_H
#define CBL_CONTAINERS_HELPERS_H

#include <memory>

namespace cbl {

template <class Map>
typename Map::mapped_type* findOrNull(Map& container, const typename Map::key_type& key) {
  auto it = container.find(key);
  return it != container.end() ? &it->second : nullptr;
}

template <class Map>
const typename Map::mapped_type* findOrNull(const Map& container, const typename Map::key_type& key) {
  auto it = container.find(key);
  return it != container.end() ? &it->second : nullptr;
}

template <class Map, typename ReturnType = std::pointer_traits<typename Map::mapped_type>::element_type*>
ReturnType findPtrOrNull(const Map& container, const typename Map::key_type& key) {
  auto it = container.find(key);
  if constexpr (std::is_same_v<typename Map::mapped_type, ReturnType>) {
    return it != container.end() ? it->second : nullptr;
  } else {
    return it != container.end() ? it->second.get() : nullptr;
  }
}

}  // namespace cbl

#endif
