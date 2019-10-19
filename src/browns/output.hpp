#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace browns {

template <std::size_t N>
std::string format_digest(const std::array<std::byte, N>& dig) {
    std::string ret;
    ret.resize(N * 2);
    for (std::size_t pos = 0; pos < N; ++pos) {
        std::byte  b       = dig[pos];
        char&      c1      = ret[pos * 2];
        char&      c2      = ret[pos * 2 + 1];
        const char tab[17] = "0123456789abcdef";
        auto       high    = (static_cast<char>(b) & 0xf0) >> 4;
        auto       low     = (static_cast<char>(b) & 0x0f);
        c1                 = tab[high];
        c2                 = tab[low];
    }
    return ret;
}

}  // namespace browns
