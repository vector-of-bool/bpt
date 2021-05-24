#pragma once

#include <neo/buffer_algorithm/copy.hpp>
#include <neo/buffer_range.hpp>

#include <bit>
#include <cstdint>

namespace dds {

/**
 * @brief Implementation of SipHash that computes a 64-bit digest
 */
class siphash64 {
    using u64 = std::uint64_t;

    u64 _digest = 0;

    /// Read a little-endien u64 integer from the given buffer
    constexpr static u64 _read_u64_le(neo::const_buffer buf) noexcept {
        std::byte bytes[8] = {};
        neo::buffer_copy(neo::mutable_buffer(bytes), buf);
        u64 m = 0;
        for (auto i = 8; i;) {
            m <<= 8;
            m |= u64(bytes[--i]);
        }
        return m;
    }

public:
    constexpr siphash64(u64 key0, u64 key1, neo::const_buffer buf) noexcept {
        u64 v0 = UINT64_C(0x736f6d6570736575);
        u64 v1 = UINT64_C(0x646f72616e646f6d);
        u64 v2 = UINT64_C(0x6c7967656e657261);
        u64 v3 = UINT64_C(0x7465646279746573);

        v0 ^= key0;
        v1 ^= key1;
        v2 ^= key0;
        v3 ^= key1;

        auto siphash_round = [&] {
            // Run one SipHash round
            v0 += v1;
            v1 = std::rotl(v1, 13);
            v1 ^= v0;
            v0 = std::rotl(v0, 32);
            v2 += v3;
            v3 = std::rotl(v3, 16);
            v3 ^= v2;
            v0 += v3;
            v3 = std::rotl(v3, 21);
            v3 ^= v0;
            v2 += v1;
            v1 = std::rotl(v1, 17);
            v1 ^= v2;
            v2 = std::rotl(v2, 32);
        };

        constexpr int c_rounds = 2;
        constexpr int d_rounds = 4;

        // Store the low byte of the size in the high bits of `b`
        u64 b = (u64)buf.size() << 56;

        while (buf.size() >= 8) {
            const auto m = _read_u64_le(buf);
            buf += 8;
            v3 ^= m;
            for (auto i = 0; i < c_rounds; ++i) {
                siphash_round();
            }
            v0 ^= m;
        }

        // Copy the remaining bytes
        switch (buf.size()) {
        case 7:
            b |= static_cast<u64>(buf[6]) << 48;
            [[fallthrough]];
        case 6:
            b |= static_cast<u64>(buf[5]) << 40;
            [[fallthrough]];
        case 5:
            b |= static_cast<u64>(buf[4]) << 32;
            [[fallthrough]];
        case 4:
            b |= static_cast<u64>(buf[3]) << 24;
            [[fallthrough]];
        case 3:
            b |= static_cast<u64>(buf[2]) << 16;
            [[fallthrough]];
        case 2:
            b |= static_cast<u64>(buf[1]) << 8;
            [[fallthrough]];
        case 1:
            b |= static_cast<u64>(buf[0]);
            break;
        case 0:
            break;
        }

        v3 ^= b;

        for (auto i = 0; i < c_rounds; ++i) {
            siphash_round();
        }

        v0 ^= b;

        v2 ^= 0xff;
        for (auto i = 0; i < d_rounds; ++i) {
            siphash_round();
        }

        _digest = v0 ^ v1 ^ v2 ^ v3;
    }

    [[nodiscard]] constexpr std::uint64_t digest() const { return _digest; }
};

}  // namespace dds
