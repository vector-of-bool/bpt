#pragma once

#include <neo/fwd.hpp>

#include <concepts>
#include <type_traits>

namespace bpt {

template <typename T>
constexpr std::decay_t<T>
decay_copy(T&& arg) noexcept requires std::constructible_from<std::decay_t<T>, T &&> {
    return static_cast<std::decay_t<T>>(NEO_FWD(arg));
}

}  // namespace bpt
