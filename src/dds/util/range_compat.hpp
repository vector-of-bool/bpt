#pragma once

#include <range/v3/range/concepts.hpp>
#include <ranges>

template <typename T>
requires std::derived_from<T, ranges::v3::view_base> constexpr inline bool
    std::ranges::enable_view<T> = true;

template <typename T>
requires std::derived_from<T, std::ranges::view_base> inline constexpr bool
    ranges::enable_view<T> = true;