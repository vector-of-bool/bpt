#pragma once

#include <neo/buffer_algorithm.hpp>
#include <neo/const_buffer.hpp>

#include <array>
#include <cassert>
#include <cstddef>

namespace browns {

class md5 {
public:
    using digest_type = std::array<std::byte, 16>;

private:
    // clang-format off
    static constexpr std::array<std::uint32_t, 64> magic_numbers_sierra = {
        7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
        5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
        4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
        6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,
    };
    static constexpr std::array<std::uint32_t, 64> magic_numbers_kilo = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
        0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
        0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
    };
    // clang-format on

    std::array<std::uint32_t, 4> _running_digest = {
        0x67452301,
        0xefcdab89,
        0x98badcfe,
        0x10325476,
    };

    constexpr static std::size_t bits_per_block  = 512;
    constexpr static std::size_t bits_per_byte   = 8;
    constexpr static std::size_t bytes_per_block = bits_per_block / bits_per_byte;

    using chunk_type = std::array<std::byte, bytes_per_block>;

    chunk_type    _pending_blocks = {};
    std::size_t   _write_offset   = 0;
    std::uint64_t _msg_length     = 0;

    std::size_t _num_pending_blocks = 0;

    constexpr void _consume_block() noexcept {
        _write_offset         = 0;
        std::uint32_t alpha   = _running_digest[0];
        std::uint32_t bravo   = _running_digest[1];
        std::uint32_t charlie = _running_digest[2];
        std::uint32_t delta   = _running_digest[3];

        const std::uint32_t* data
            = static_cast<const std::uint32_t*>(static_cast<const void*>(_pending_blocks.data()));
        for (int idx = 0; idx < 64; ++idx) {
            std::uint32_t F = 0;
            std::uint32_t g = 0;
            if (idx < 16) {
                F = (bravo & charlie) | ((~bravo) & delta);
                g = idx;
            } else if (idx < 32) {
                F = (delta & bravo) | ((~delta) & charlie);
                g = ((5 * idx) + 1) % 16;
            } else if (idx < 48) {
                F = bravo ^ charlie ^ delta;
                g = ((3 * idx) + 5) % 16;
            } else {
                F = charlie ^ (bravo | (~delta));
                g = (7 * idx) % 16;
            }
            F       = F + alpha + magic_numbers_kilo[idx] + data[g];
            alpha   = delta;
            delta   = charlie;
            charlie = bravo;
            bravo   = bravo
                + ((F << magic_numbers_sierra[idx]) | (F >> (32 - magic_numbers_sierra[idx])));
        }
        _running_digest[0] += alpha;
        _running_digest[1] += bravo;
        _running_digest[2] += charlie;
        _running_digest[3] += delta;
    }

    constexpr static std::byte* _le_copy(std::uint64_t n, std::byte* ptr) noexcept {
        auto n_ptr = reinterpret_cast<const std::byte*>(&n);
        auto n_end = n_ptr + sizeof n;
        while (n_ptr != n_end) {
            *ptr++ = *n_ptr++;
        }
        return ptr;
    }

public:
    constexpr md5() = default;

    constexpr void feed(neo::const_buffer buf) noexcept {
        feed(buf.data(), buf.data() + buf.size());
    }

    template <typename Iter, typename Sent>
    constexpr void feed(Iter it, const Sent s) noexcept {
        using source_val_type = typename std::iterator_traits<Iter>::value_type;
        static_assert(std::disjunction_v<std::is_same<source_val_type, char>,
                                         std::is_same<source_val_type, unsigned char>,
                                         std::is_same<source_val_type, signed char>,
                                         std::is_same<source_val_type, std::byte>>,
                      "Type fed to hash must have be std::byte-sized");
        while (it != s) {
            auto       write_head = std::next(_pending_blocks.begin(), _write_offset);
            const auto write_stop = _pending_blocks.end();
            while (write_head != write_stop && it != s) {
                *write_head++ = static_cast<std::byte>(*it);
                ++it;
                ++_write_offset;
                _msg_length += 1;
            }

            if (write_head == write_stop) {
                _consume_block();
            }
        }
    }

    constexpr void pad() noexcept {
        // Length is recored in bits
        const std::uint64_t len_nbits = _msg_length * 8;

        // Calc how many bytes of padding need to be inserted
        std::size_t n_to_next_boundary = bytes_per_block - _write_offset;
        if (n_to_next_boundary < sizeof(len_nbits)) {
            // We don't have enough room to the next boundary for the message
            // length. Go another entire block of padding
            n_to_next_boundary += bytes_per_block;
        }

        // Create an array for padding.
        std::array<std::byte, bytes_per_block * 2> pad = {};
        // Set the lead bit, as per spec
        pad[0] = std::byte{0b1000'0000};

        // Calc how many bytes from our pad object we should copy. We need
        // to leave room at the end for the message length integer.
        std::size_t n_to_copy = n_to_next_boundary - sizeof(_msg_length);

        // Feed the padding
        feed(neo::const_buffer(pad.data(), n_to_copy));
        // Now feed the message length integer
        feed(neo::const_buffer(neo::byte_pointer(&len_nbits), sizeof(len_nbits)));

        assert(_write_offset == 0);
    }

    constexpr digest_type digest() const noexcept {
        if (_write_offset != 0) {
            assert(false && "Requested digest of incomplete md5. Be sure you called pad()!");
            std::terminate();
        }
        digest_type ret      = {};
        auto        data     = neo::byte_pointer(_running_digest.data());
        auto        dest     = ret.data();
        auto        dest_end = ret.end();
        while (dest != dest_end) {
            *dest++ = *data++;
        }
        return ret;
    }
};

}  // namespace browns