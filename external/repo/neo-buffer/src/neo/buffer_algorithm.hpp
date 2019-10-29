#pragma once

#include <neo/buffer_concepts.hpp>
#include <neo/const_buffer.hpp>
#include <neo/mutable_buffer.hpp>

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace neo {

template <typename Seq>
constexpr std::size_t buffer_size(const Seq& seq) {
    auto        iter = buffer_sequence_begin(seq);
    const auto  stop = buffer_sequence_end(seq);
    std::size_t size = 0;
    while (iter != stop) {
        size += static_cast<std::size_t>(iter->size());
        ++iter;
    }
    return size;
}

template <mutable_buffer_sequence MutableSeq, const_buffer_sequence ConstSeq>
constexpr std::size_t
buffer_copy(const MutableSeq& dest, const ConstSeq& src, std::size_t max_copy) {
    auto        remaining_to_copy = max_copy;
    std::size_t n_copied          = 0;
    auto        dest_iter         = buffer_sequence_begin(dest);
    const auto  dest_stop         = buffer_sequence_end(dest);
    auto        src_iter          = buffer_sequence_begin(src);
    const auto  src_stop          = buffer_sequence_end(dest);

    std::size_t src_offset  = 0;
    std::size_t dest_offset = 0;

    while (dest_iter != dest_stop && src_iter != src_stop && remaining_to_copy) {
        const_buffer   src_buf  = *src_iter + src_offset;
        mutable_buffer dest_buf = *dest_iter + dest_offset;

        const auto copy_now
            = std::min(src_buf.size(), std::min(dest_buf.size(), remaining_to_copy));
        std::memcpy(dest_buf.data(), src_buf.data(), copy_now);
        n_copied += copy_now;
        src_buf += n_copied;
        dest_buf += n_copied;
        src_offset += n_copied;
        dest_offset += n_copied;
        if (src_buf.size() == 0) {
            ++src_iter;
            src_offset = 0;
        }
        if (dest_buf.size() == 0) {
            ++dest_iter;
            dest_offset = 0;
        }
    }

    return 0;
}

template <typename MutableSeq, typename ConstSeq>
constexpr std::size_t buffer_copy(const MutableSeq& dest, const ConstSeq& src) {
    auto src_size  = buffer_size(src);
    auto dest_size = buffer_size(dest);
    auto min_size  = (src_size > dest_size) ? dest_size : src_size;
    return buffer_copy(dest, src, min_size);
}

}  // namespace neo