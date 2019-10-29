#include <dds/toolchain/toolchain.hpp>

#include <dds/util.test.hpp>

using namespace dds;

namespace {

#define CHECK_SHLEX(str, ...)                                                                      \
    do {                                                                                           \
        CHECK(dds::split_shell_string(str) == std::vector<std::string>(__VA_ARGS__));              \
    } while (0)

void test_shlex() {
    CHECK_SHLEX("foo", {"foo"});
    CHECK_SHLEX("foo bar", {"foo", "bar"});
    CHECK_SHLEX("\"foo\" bar", {"foo", "bar"});
    CHECK_SHLEX("\"foo bar\"", {"foo bar"});
    CHECK_SHLEX("", {});
    CHECK_SHLEX("   ", {});
    CHECK_SHLEX("\"\"", {""});
    CHECK_SHLEX("'quoted arg'", {"quoted arg"});
    CHECK_SHLEX("word     ", {"word"});
    CHECK_SHLEX("\"     meow\"", {"     meow"});
    CHECK_SHLEX("foo     bar", {"foo", "bar"});
    CHECK_SHLEX("C:\\\\Program\\ Files", {"C:\\Program Files"});
    CHECK_SHLEX("Foo\nBar", {"Foo", "Bar"});
    CHECK_SHLEX("foo \"\" bar", {"foo", "", "bar"});
}

void run_tests() { test_shlex(); }

}  // namespace

DDS_TEST_MAIN;
