#pragma once

#include <neo/const_buffer.hpp>
#include <neo/mutable_buffer.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <type_traits>

namespace neo {

constexpr mutable_buffer buffer(const mutable_buffer& b) noexcept { return b; }
constexpr mutable_buffer buffer(const mutable_buffer& b, mutable_buffer::size_type s) noexcept {
    auto min_size = s > b.size() ? b.size() : s;
    return mutable_buffer(b.data(), min_size);
}

constexpr const_buffer buffer(const const_buffer& b) noexcept { return b; }
constexpr const_buffer buffer(const const_buffer& b, const_buffer::size_type s) noexcept {
    auto min_size = s > b.size() ? b.size() : s;
    return const_buffer(b.data(), min_size);
}

// #############################################################################
/**
 * Create a mutable buffer from an opaque pointer to void
 */
constexpr mutable_buffer buffer(void* ptr, mutable_buffer::size_type s) noexcept {
    return mutable_buffer(static_cast<std::byte*>(ptr), s);
}

/**
 * Create an immutable buffer from an opaque pointer to void
 */
constexpr const_buffer buffer(const void* ptr, const_buffer::size_type s) noexcept {
    return const_buffer(static_cast<const std::byte*>(ptr), s);
}

// #############################################################################
/**
 * Create a mutable buffer that refers to the bytes of a trivial object.
 */
template <typename Trivial,
          typename
          = std::enable_if_t<!std::is_const_v<Trivial> && !std::is_same_v<Trivial, mutable_buffer>>>
constexpr mutable_buffer buffer(Trivial& item, std::size_t max_size = sizeof(Trivial)) {
    auto min_size = max_size > sizeof(item) ? sizeof(item) : max_size;
    return mutable_buffer(byte_pointer(std::addressof(item)), min_size);
}

/**
 * Create an immutable buffer that refers to the bytes of a trivial object.
 */
template <
    typename Trivial,
    typename = std::enable_if_t<
        std::is_const_v<
            Trivial> && !std::is_same_v<Trivial, mutable_buffer> && !std::is_same_v<Trivial, const_buffer>>>
constexpr const_buffer buffer(Trivial& item, std::size_t max_size = sizeof(Trivial)) {
    auto min_size = std::min(sizeof(item), max_size);
    return const_buffer(byte_pointer(std::addressof(item)), min_size);
}

// #############################################################################
/**
 * Create a mutable buffer referring to the characters of a basic_string object
 */
template <typename Char, typename Traits, typename Alloc>
constexpr mutable_buffer buffer(std::basic_string<Char, Traits, Alloc>& str, std::size_t max_size) {
    auto use_size = max_size > str.size() ? str.size() : max_size;
    return buffer(str.data(), use_size);
}

template <typename Char, typename Traits, typename Alloc>
constexpr mutable_buffer buffer(std::basic_string<Char, Traits, Alloc>& str) {
    return buffer(str, str.size());
}

/**
 * Create an immutable buffer refering to the characters of a basic_string object
 */
template <typename Char, typename Traits, typename Alloc>
constexpr const_buffer buffer(const std::basic_string<Char, Traits, Alloc>& str,
                              std::size_t                                   max_size) {
    auto use_size = max_size > str.size() ? str.size() : max_size;
    return buffer(str.data(), use_size);
}

template <typename Char, typename Traits, typename Alloc>
constexpr const_buffer buffer(const std::basic_string<Char, Traits, Alloc>& str) {
    return buffer(str, str.size());
}

// #############################################################################
/**
 * Create an immutable buffer referring to the characters of a basic_string_view
 */
template <typename Char, typename Traits>
constexpr const_buffer buffer(std::basic_string_view<Char, Traits> sv, std::size_t max_size) {
    auto use_size = max_size > sv.size() ? sv.size() : max_size;
    return buffer(sv.data(), use_size);
}

template <typename Char, typename Traits>
constexpr const_buffer buffer(std::basic_string_view<Char, Traits> sv) {
    return buffer(sv, sv.size());
}

}  // namespace neo