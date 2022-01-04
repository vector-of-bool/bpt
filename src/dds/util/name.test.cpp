#include "./name.hpp"

#include <dds/error/result.hpp>

#include <boost/leaf.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Try some invalid names") {
    using reason = dds::invalid_name_reason;
    struct case_ {
        std::string_view         invalid_name;
        dds::invalid_name_reason error;
    };
    auto given = GENERATE(Catch::Generators::values<case_>({
        {"", reason::empty},

        {"H", reason::capital},
        {"heLlo", reason::capital},
        {"eGGG", reason::capital},
        {"e0131F-gg", reason::capital},

        {"-foo", reason::initial_not_alpha},
        {"123", reason::initial_not_alpha},
        {"123-bar", reason::initial_not_alpha},
        {" fooo", reason::initial_not_alpha},

        {"foo..bar", reason::double_punct},
        {"foo-.bar", reason::double_punct},
        {"foo-_bar", reason::double_punct},
        {"foo__bar", reason::double_punct},
        {"foo_.bar", reason::double_punct},

        {"foo.", reason::end_punct},
        {"foo.bar_", reason::end_punct},
        {"foo.bar-", reason::end_punct},

        {"foo ", reason::whitespace},
        {"foo bar", reason::whitespace},
        {"foo\nbar", reason::whitespace},

        {"foo&bar", reason::invalid_char},
        {"foo+baz", reason::invalid_char},
    }));

    boost::leaf::context<reason> err_ctx;
    err_ctx.activate();
    CAPTURE(given.invalid_name);
    auto res = dds::name::from_string(given.invalid_name);
    err_ctx.deactivate();
    CHECKED_IF(!res) {
        err_ctx.handle_error<void>(
            res.error(),
            [&](reason r) { CHECK(r == given.error); },
            [] { FAIL_CHECK("No error reason was given"); });
    }
}

TEST_CASE("Try some valid names") {
    auto given = GENERATE(Catch::Generators::values<std::string_view>({
        "hi",
        "dog",
        "foo.bar",
        "foo-bar_bark",
        "foo-bar-baz",
        "foo-bar.quz",
        "q",
    }));

    auto res = dds::name::from_string(given);
    CHECK(res->str == given);
}
