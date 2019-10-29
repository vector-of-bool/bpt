#include <neo/buffer_algorithm.hpp>

#include <neo/buffer.test.hpp>

#include <neo/const_buffer.hpp>

#include <string_view>

int main() {
    auto buf      = neo::const_buffer("A string");
    auto buf_iter = neo::buffer_sequence_begin(buf);
    CHECK(buf_iter->data() == buf.data());
    CHECK(buf_iter != neo::buffer_sequence_end(buf));

    CHECK(neo::buffer_size(buf) == buf.size());

    // neo::buffer_size(12);
}
