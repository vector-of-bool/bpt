#include <neo/buffer.hpp>

#include <neo/buffer.test.hpp>
#include <neo/buffer_algorithm.hpp>

#include <array>
#include <string>

struct my_simple_struct {
    int a;
    int b;
};

int main() {
    // std::string s = "I am a string!";
    // auto s_buf = neo::buffer(s);
    // CHECK(s_buf.data() == s.data());

    my_simple_struct foo;
    foo.a = 12;
    foo.b = 3;

    neo::mutable_buffer pod_buf = neo::buffer(foo);
    CHECK(pod_buf.size() == sizeof(foo));

    my_simple_struct bar;
    neo::buffer_copy(neo::buffer(bar), pod_buf);
    CHECK(bar.a == foo.a);
    CHECK(bar.b == foo.b);

    bar.b = 55;

    std::array<char, sizeof bar> buf;
    neo::buffer_copy(neo::buffer(buf), neo::buffer(bar));

    neo::buffer_copy(neo::buffer(foo), neo::buffer(buf));
    CHECK(foo.b == 55);
}