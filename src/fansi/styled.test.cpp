#include "./styled.hpp"

#include <catch2/catch.hpp>

static std::string render(std::string_view fmt) {
    return fansi::stylize(fmt, fansi::should_style::force);
}

TEST_CASE("Stylize some text") {
    auto test = render("foo bar");
    CHECK(test == "foo bar");
    test = render("foo. bar.");
    CHECK(test == "foo. bar.");
    test = render("foo `.eggs");
    CHECK(test == "foo .eggs");

    test = render("foo `.bar[`]");
    CHECK(test == "foo .bar[]");

    test = render("foo .bold[bar] baz");
    CHECK(test == "foo \x1b[1mbar\x1b[0m baz");

    test = render("foo .bold.red[bar] baz");
    CHECK(test == "foo \x1b[1;31mbar\x1b[0m baz");

    test = render("foo .br.red[bar] baz");
    CHECK(test == "foo \x1b[91mbar\x1b[0m baz");

    test = render("foo .br.italic[bar] baz");
    CHECK(test == "foo \x1b[3mbar\x1b[0m baz");

    test = render("foo .red[I am a string with .bold[bold] text inside]");
    CHECK(test == "foo \x1b[31mI am a string with \x1b[1mbold\x1b[0;31m text inside\x1b[0m");
}
