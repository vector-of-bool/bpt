#pragma once

#include <neo/base_buffer.hpp>
#include <neo/mutable_buffer.hpp>

#include <cassert>
#include <cstddef>
#include <string_view>

namespace neo {

/**
 * A type that represents a view to a readonly segment of contiguous memory.
 */
class const_buffer {
public:
    using pointer   = const std::byte*;
    using size_type = std::size_t;

private:
    pointer   _data = nullptr;
    size_type _size = 0;

public:
    constexpr const_buffer() noexcept = default;
    constexpr const_buffer(pointer ptr, size_type size) noexcept
        : _data(ptr)
        , _size(size) {}

    constexpr const_buffer(mutable_buffer buf)
        : _data(buf.data())
        , _size(buf.size()) {}

    explicit constexpr const_buffer(std::string_view sv)
        : _data(byte_pointer(sv.data()))
        , _size(sv.size()) {}

    constexpr pointer   data() const noexcept { return _data; }
    constexpr pointer   data_end() const noexcept { return _data + size(); }
    constexpr size_type size() const noexcept { return _size; }

    constexpr const_buffer& operator+=(size_type s) noexcept {
        assert(s <= size() && "Advanced neo::const_buffer past-the-end");
        _data += s;
        _size -= s;
        return *this;
    }
};

inline constexpr const_buffer operator+(const_buffer buf, const_buffer::size_type s) noexcept {
    auto copy = buf;
    copy += s;
    return copy;
}

inline constexpr auto _impl_buffer_sequence_begin(const_buffer buf) noexcept {
    return detail::single_buffer_iter(buf);
}

inline constexpr auto _impl_buffer_sequence_end(const_buffer) noexcept {
    return detail::single_buffer_iter_sentinel();
}

}  // namespace neo