#pragma once

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <vector>

namespace bpt {

template <typename Container, typename Predicate>
void erase_if(Container& c, Predicate&& p) {
    auto erase_point = std::remove_if(c.begin(), c.end(), p);
    c.erase(erase_point, c.end());
}

template <typename Container, typename Iter, typename Stop>
void extend(Container& c, Iter iter, const Stop stop) {
    while (iter != stop) {
        c.insert(c.end(), typename Container::value_type(*iter++));
    }
}

template <typename Container, typename Iter>
void extend(Container& c, Iter iter, Iter end) requires requires {
    c.insert(c.end(), iter, end);
}
{ c.insert(c.end(), iter, end); }

template <typename Container, typename Iter>
void extend(Container& c, Iter iter, Iter end) requires requires {
    c.insert(iter, end);
}
{ c.insert(iter, end); }

template <typename Container, typename Other>
void extend(Container& c, Other&& o) {
    extend(c, o.begin(), o.end());
}

template <typename Container, typename Item>
void extend(Container& c, std::initializer_list<Item> il) {
    c.insert(c.end(), il.begin(), il.end());
}

template <typename Container>
void unique_erase(Container& c) noexcept {
    c.erase(std::unique(c.begin(), c.end()), c.end());
}

template <typename Container>
void sort_unique_erase(Container& c) noexcept {
    std::sort(c.begin(), c.end());
    unique_erase(c);
}

template <typename T>
using ref_vector = std::vector<std::reference_wrapper<T>>;

}  // namespace bpt
