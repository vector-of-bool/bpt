#include "./debate.hpp"

#include "./enum.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Create an argument parser") {
    enum log_level {
        _invalid,
        info,
        warning,
        error,
    };
    log_level level;

    std::string file;

    debate::argument_parser parser;
    parser.add_argument(debate::argument{
        .long_spellings  = {"log-level"},
        .short_spellings = {"l"},
        .help            = "Set the log level",
        .valname         = "<level>",
        .action          = debate::put_into(level),
    });
    parser.add_argument(debate::argument{
        .help    = "A file to read",
        .valname = "<file>",
        .action  = debate::put_into(file),
    });
    parser.parse_argv({"--log-level=info"});
    CHECK(level == log_level::info);
    parser.parse_argv({"--log-level=warning"});
    CHECK(level == log_level::warning);
    parser.parse_argv({"--log-level", "info"});
    CHECK(level == log_level::info);
    parser.parse_argv({"-lerror"});
    CHECK(level == log_level::error);
    CHECK_THROWS_AS(parser.parse_argv({"-lerror", "--log-level=info"}), std::runtime_error);

    parser.parse_argv({"-l", "info"});
    CHECK(level == log_level::info);

    parser.parse_argv({"-lwarning", "my-file.txt"});
    CHECK(level == log_level::warning);
    CHECK(file == "my-file.txt");
}

TEST_CASE("Subcommands") {
    std::optional<bool> do_eat;
    std::optional<bool> scramble_eggs;
    std::string_view    subcommand;

    debate::argument_parser parser;
    parser.add_argument({
        .long_spellings = {"eat"},
        .nargs          = 0,
        .action         = debate::store_true(do_eat),
    });

    auto& sub = parser.add_subparsers(debate::subparser_group{.valname = "<food>"});
    auto& egg_parser
        = sub.add_parser(debate::subparser{.name   = "egg",
                                           .help   = "It's an egg",
                                           .action = debate::store_value(subcommand, "egg")});
    egg_parser.add_argument(
        {.long_spellings = {"scramble"}, .nargs = 0, .action = debate::store_true(scramble_eggs)});

    parser.parse_argv({"egg"});
    parser.parse_argv({"--eat", "egg"});
    // Missing the subcommand:
    CHECK_THROWS_AS(parser.parse_argv({"--eat"}), std::runtime_error);
    CHECK_FALSE(scramble_eggs);
    parser.parse_argv({"egg", "--scramble"});
    CHECK(scramble_eggs);
    CHECK(subcommand == "egg");

    do_eat.reset();
    scramble_eggs.reset();
    subcommand = {};
    parser.parse_argv({"egg", "--scramble", "--eat"});
    CHECK(do_eat);
    CHECK(scramble_eggs);
    CHECK(subcommand == "egg");
}
