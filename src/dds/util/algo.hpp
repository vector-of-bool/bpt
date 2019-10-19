#pragma once

#include <algorithm>
#include <initializer_list>

namespace dds {

template <typename Container, typename Predicate>
void erase_if(Container& c, Predicate&& p) {
    auto erase_point = std::remove_if(c.begin(), c.end(), p);
    c.erase(erase_point, c.end());
}

template <typename Container, typename Other>
void extend(Container& c, const Other& o) {
    c.insert(c.end(), o.begin(), o.end());
}

template <typename Container, typename Item>
void extend(Container& c, std::initializer_list<Item> il) {
    c.insert(c.end(), il.begin(), il.end());
}

}  // namespace dds