#pragma once

#include "./style.hpp"

#include <neo/as_buffer.hpp>
#include <neo/as_dynamic_buffer.hpp>
#include <neo/buffer_algorithm/size.hpp>
#include <neo/buffer_range.hpp>

#include <initializer_list>
#include <string>

namespace fansi {

class text_writer {
    std::string _buf;
    std::size_t _vis_size = 0;

    text_style _style;

    template <neo::buffer_range Bufs>
    void _write_raw(Bufs&& bufs, std::size_t s) noexcept {
        auto out = neo::as_dynamic_buffer(_buf).grow(s);
        neo::buffer_copy(out, bufs);
    }

    template <neo::buffer_range Bufs>
    void _write(Bufs&& bufs) noexcept {
        auto size = neo::buffer_size(bufs);
        _write_raw(bufs, size);
        _vis_size += size;
    }

public:
    template <neo::buffer_range Buf>
    void write(Buf&& bufs) noexcept {
        _write(bufs);
    }

    template <neo::as_buffer_convertible B>
    requires(!neo::buffer_range<B>) void write(B&& b) noexcept {
        auto bufs = {neo::as_buffer(b)};
        _write(bufs);
    }

    void write(std::initializer_list<neo::const_buffer> bufs) noexcept { return _write(bufs); }

    void putc(char c) noexcept { write(std::string_view(&c, 1)); }

    void put_style(const text_style&) noexcept;

    std::string      take_string() noexcept { return std::move(_buf); }
    std::string_view string() const noexcept { return _buf; }
    auto             visual_size() const noexcept { return _vis_size; }
};

}  // namespace fansi
