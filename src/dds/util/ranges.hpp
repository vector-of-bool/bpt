#pragma once

namespace dds {

template <typename T>
constexpr auto& view_safe(T&& t) {
    return t;
}

}  // namespace dds