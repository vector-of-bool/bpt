#include <bpt/util/shlex.hpp>

#include <catch2/catch.hpp>

#define CHECK_SHLEX(str, ...)                                                                      \
    do {                                                                                           \
        INFO("Shell-lexing string: '" << str << "'");                                              \
        CHECK(bpt::split_shell_string(str) == std::vector<std::string>(__VA_ARGS__));              \
    } while (0)

TEST_CASE("Shell lexer") {
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
