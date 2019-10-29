#pragma once

#include <utility>

namespace neo {

constexpr inline struct buffer_sequence_begin_fn {
    template <typename T>
    decltype(auto) operator()(T&& t) const requires requires {
        buffer_sequence_begin(t);
    }
    { return buffer_sequence_begin(t); }

    template <typename T>
    decltype(auto) operator()(T&& t) const requires requires {
        t.buffer_sequence_begin();
    }
    { return t.buffer_sequence_begin(); }

    template <typename T>
    decltype(auto) operator()(T&& t) const requires requires {
        _impl_buffer_sequence_begin(t);
    }
    { return _impl_buffer_sequence_begin(t); }
} buffer_sequence_begin;

inline struct buffer_sequence_end_fn {
    template <typename T>
    decltype(auto) operator()(T&& t) const requires requires {
        buffer_sequence_end(t);
    }
    { return buffer_sequence_end(t); }

    template <typename T>
    decltype(auto) operator()(T&& t) const requires requires {
        t.buffer_sequence_end();
    }
    { return t.buffer_sequence_end(); }

    template <typename T>
    decltype(auto) operator()(T&& t) const requires requires {
        _impl_buffer_sequence_end(t);
    }
    { return _impl_buffer_sequence_end(t); }
} buffer_sequence_end;

}  // namespace neo