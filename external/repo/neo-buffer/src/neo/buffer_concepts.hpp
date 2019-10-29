#pragma once

#ifndef NEO_CONCEPT
#if defined(__GNUC__) && __GNUC__ < 9
#define NEO_CONCEPT concept bool
#else
#define NEO_CONCEPT concept
#endif
#endif

#include <neo/buffer_seq_iter.hpp>
#include <neo/const_buffer.hpp>
#include <neo/mutable_buffer.hpp>

#include <iterator>

namespace neo {

template <typename T>
NEO_CONCEPT const_buffer_sequence_iterator = requires(T iter) {
    typename std::iterator_traits<T>::value_type;
    std::is_convertible_v<typename std::iterator_traits<T>::value_type, const_buffer>;
};

template <typename T>
NEO_CONCEPT mutable_buffer_sequence_iterator
    = const_buffer_sequence_iterator<T>&& requires(T iter) {
    std::is_convertible_v<typename std::iterator_traits<T>::value_type, mutable_buffer>;
};

template <typename T>
NEO_CONCEPT const_buffer_sequence = requires(T seq) {
    { buffer_sequence_begin(seq) }
    ->const_buffer_sequence_iterator;
    buffer_sequence_end(seq);
    { buffer_sequence_begin(seq) != buffer_sequence_end(seq) }
    ->bool;
};

template <typename T>
NEO_CONCEPT mutable_buffer_sequence = const_buffer_sequence<T>&& requires(T seq) {
    { buffer_sequence_begin(seq) }
    ->mutable_buffer_sequence_iterator;
};

}  // namespace neo