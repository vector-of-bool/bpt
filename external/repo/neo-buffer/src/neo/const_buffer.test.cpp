#include <neo/buffer.test.hpp>
#include <neo/const_buffer.hpp>

#include <iostream>

int main() {
    neo::const_buffer buf;
    CHECK(buf.size() == 0);

    buf = neo::const_buffer("meow");
    CHECK(buf.size() == 4);
    auto buf2 = buf + 3;
    CHECK(buf2.size() == 1);
    CHECK(buf2.data()[0] == std::byte('w'));
    buf2 += 1;
    CHECK(buf2.size() == 0);
}
