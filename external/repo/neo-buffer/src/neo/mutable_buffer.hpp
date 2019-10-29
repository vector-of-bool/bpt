#pragma once

#include <neo/base_buffer.hpp>

#include <cstddef>
#include <iterator>
#include <type_traits>

namespace neo {

class mutable_buffer {
public:
    using pointer   = std::byte*;
    using size_type = std::size_t;

private:
    pointer   _data = nullptr;
    size_type _size = 0;

public:
    constexpr mutable_buffer() noexcept = default;
    constexpr mutable_buffer(pointer p, size_type size) noexcept
        : _data(p)
        , _size(size) {}

    constexpr mutable_buffer& operator+=(size_type s) noexcept {
        _data += s;
        _size -= s;
        return *this;
    }

    constexpr pointer   data() const noexcept { return _data; }
    constexpr pointer   data_end() const noexcept { return _data + size(); }
    constexpr size_type size() const noexcept { return _size; }
};

inline constexpr mutable_buffer operator+(mutable_buffer            buf,
                                          mutable_buffer::size_type s) noexcept {
    auto copy = buf;
    copy += s;
    return copy;
}

inline constexpr auto _impl_buffer_sequence_begin(mutable_buffer buf) noexcept {
    return detail::single_buffer_iter(buf);
}

inline constexpr auto _impl_buffer_sequence_end(mutable_buffer) noexcept {
    return detail::single_buffer_iter_sentinel();
}

}  // namespace neo