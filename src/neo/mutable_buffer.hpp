#pragma once

#include <cstddef>
#include <iterator>
#include <type_traits>

namespace neo {

template <typename T, typename = std::enable_if_t<std::is_trivial_v<T>>>
constexpr std::byte* byte_pointer(T* ptr) {
    auto void_ = static_cast<void*>(ptr);
    return static_cast<std::byte*>(void_);
}

template <typename T, typename = std::enable_if_t<std::is_trivial_v<T>>>
constexpr const std::byte* byte_pointer(const T* ptr) noexcept {
    auto void_ = static_cast<const void*>(ptr);
    return static_cast<const std::byte*>(void_);
}

class mutable_buffer {
public:
    using pointer   = std::byte*;
    using size_type = std::size_t;

private:
    pointer   _ptr  = nullptr;
    size_type _size = 0;

public:
    constexpr mutable_buffer() noexcept = default;
    constexpr mutable_buffer(pointer p, size_type size) noexcept
        : _ptr(p)
        , _size(size) {}

    constexpr mutable_buffer& operator+=(size_type s) noexcept {
        _ptr += s;
        _size -= s;
        return *this;
    }

    constexpr pointer   data() const noexcept { return _ptr; }
    constexpr size_type size() const noexcept { return _size; }
};

inline constexpr mutable_buffer operator+(mutable_buffer            buf,
                                          mutable_buffer::size_type s) noexcept {
    auto copy = buf;
    copy += s;
    return copy;
}

namespace detail {

class single_buffer_iter_sentinel {};

template <typename Buffer>
class single_buffer_iter {
    Buffer _buf;
    bool   _dead = false;

public:
    using difference_type   = std::ptrdiff_t;
    using value_type        = Buffer;
    using pointer           = const value_type*;
    using reference         = const value_type&;
    using iterator_category = std::forward_iterator_tag;

    constexpr single_buffer_iter(Buffer b) noexcept
        : _buf(b) {}

    constexpr reference operator*() const noexcept { return _buf; }
    constexpr pointer   operator->() const noexcept { return &_buf; }

    constexpr bool operator==(single_buffer_iter_sentinel) const noexcept { return _dead; }
    constexpr bool operator!=(single_buffer_iter_sentinel) const noexcept { return !_dead; }
    constexpr bool operator==(single_buffer_iter o) const noexcept { return _dead == o._dead; }
    constexpr bool operator!=(single_buffer_iter o) const noexcept { return !(*this == o); }

    constexpr single_buffer_iter operator++(int) noexcept {
        auto me = *this;
        _dead   = true;
        return me;
    }

    constexpr single_buffer_iter& operator++() noexcept {
        _dead = true;
        return *this;
    }
};

}  // namespace detail

inline auto get_buffer_sequence_begin(mutable_buffer b) noexcept {
    return detail::single_buffer_iter(b);
}

inline constexpr auto get_buffer_sequence_end(mutable_buffer) noexcept {
    return detail::single_buffer_iter_sentinel();
}

}  // namespace neo