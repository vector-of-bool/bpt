#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

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

template <typename Digest>
Digest parse_digest(std::string_view str) {
    Digest           ret;
    std::byte*       out_ptr = ret.data();
    std::byte* const out_end = out_ptr + ret.size();
    auto             str_ptr = str.begin();
    const auto       str_end = str.end();

    auto invalid = [&] {  //
        throw std::runtime_error("Invalid hash digest string: " + std::string(str));
    };

    auto nibble = [&](char c) -> std::byte {
        if (c >= 'A' && c <= 'F') {
            c = static_cast<char>(c + ('a' - 'A'));
        }
        if (c >= '0' && c <= '9') {
            return std::byte(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            return std::byte(c - 'a');
        } else {
            invalid();
        }
        std::terminate();
    };

    // We must have an even number of chars to form full octets
    if (str.size() % 2) {
        invalid();
    }

    while (str_ptr != str_end && out_ptr != out_end) {
        std::byte high  = nibble(*str_ptr++);
        std::byte low   = nibble(*str_ptr++);
        std::byte octet = (high << 4) | low;
        *out_ptr++      = octet;
    }

    if (str_ptr != str_end || out_ptr != out_end) {
        invalid();
    }
    return ret;
}

}  // namespace browns
