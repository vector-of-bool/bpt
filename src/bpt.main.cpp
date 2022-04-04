#include <bpt/cli/dispatch_main.hpp>
#include <bpt/cli/options.hpp>
#include <bpt/util/env.hpp>
#include <bpt/util/log.hpp>
#include <bpt/util/output.hpp>
#include <bpt/util/signal.hpp>

#include <debate/debate.hpp>
#include <debate/enum.hpp>

#include <boost/leaf/pred.hpp>
#include <fansi/styled.hpp>
#include <fmt/ostream.h>
#include <neo/event.hpp>

#include <clocale>
#include <filesystem>
#include <iostream>
#include <locale>

using namespace fansi::literals;

static void load_locale() {
    auto lang = bpt::getenv("LANG");
    if (!lang) {
        return;
    }
    try {
        std::locale::global(std::locale(*lang));
    } catch (const std::runtime_error& e) {
        // No locale with the given name
        return;
    }
}

int main_fn(std::string_view program_name, const std::vector<std::string>& argv) {
    bpt::log::init_logger();
    neo::listener log_listener = &bpt::log::ev_log::print;
    load_locale();
    std::setlocale(LC_CTYPE, ".utf8");

    bpt::enable_ansi_console();

    bpt::cli::options       opts;
    debate::argument_parser parser;
    opts.setup_parser(parser);

    auto result = boost::leaf::try_catch(
        [&]() -> std::optional<int> {
            parser.parse_argv(argv);
            return std::nullopt;
        },
        [&](debate::help_request, debate::e_argument_parser p) {
            std::cout << p.value.help_string(program_name);
            return 0;
        },
        [&](debate::unrecognized_argument,
            debate::e_argument_parser p,
            debate::e_arg_spelling    arg,
            debate::e_did_you_mean*   dym) {
            std::cerr << p.value.usage_string(program_name) << '\n';
            if (p.value.subparsers()) {
                fmt::print(std::cerr,
                           "Unrecognized argument/subcommand: \".bold.red[{}]\"\n"_styled,
                           arg.value);
            } else {
                fmt::print(std::cerr,
                           "Unrecognized argument: \".bold.red[{}]\"\n"_styled,
                           arg.value);
            }
            if (dym) {
                fmt::print(std::cerr, "  (Did you mean '.br.yellow[{}]'?)\n"_styled, dym->value);
            }
            return 2;
        },
        [&](debate::invalid_arguments,
            debate::e_argument          arg,
            debate::e_argument_parser   p,
            debate::e_arg_spelling      spell,
            debate::e_invalid_arg_value val) {
            std::cerr << p.value.usage_string(program_name) << '\n';
            fmt::print(std::cerr,
                       "Invalid {} value '{}' given for '{}'\n",
                       arg.value.valname,
                       val.value,
                       spell.value);
            return 2;
        },
        [&](debate::invalid_arguments,
            debate::e_argument_parser p,
            debate::e_arg_spelling    spell,
            debate::e_argument        arg,
            debate::e_wrong_val_num   given) {
            std::cerr << p.value.usage_string(program_name) << '\n';
            if (arg.value.nargs == 0) {
                fmt::print(std::cerr,
                           "Argument '{}' does not expect any values, but was given one\n",
                           spell.value);
            } else if (arg.value.nargs == 1 && given.value == 0) {
                fmt::print(std::cerr,
                           "Argument '{}' expected to be given a value, but received none\n",
                           spell.value);
            } else {
                fmt::print(
                    std::cerr,
                    "Wrong number of arguments provided for '{}': Expected {}, but only got {}\n",
                    spell.value,
                    arg.value.nargs,
                    given.value);
            }
            return 2;
        },
        [&](debate::missing_required, debate::e_argument_parser p, debate::e_argument arg) {
            fmt::print(std::cerr,
                       "{}\nMissing required argument '{}'\n",
                       p.value.usage_string(program_name),
                       arg.value.preferred_spelling());
            return 2;
        },
        [&](debate::invalid_repetition, debate::e_argument_parser p, debate::e_arg_spelling sp) {
            fmt::print(std::cerr,
                       "{}\nArgument '{}' cannot be provided more than once\n",
                       p.value.usage_string(program_name),
                       sp.value);
            return 2;
        },
        [&](debate::invalid_arguments const& err, debate::e_argument_parser p) {
            fmt::print(std::cerr,
                       "{}\nError: {}\n",
                       p.value.usage_string(program_name),
                       err.what());
            return 2;
        });
    if (result) {
        // Non-null result from argument parsing, return that value immediately.
        return *result;
    }
    if (opts.subcommand != bpt::cli::subcommand::new_) {
        // We want ^C to behave as-normal for 'new'
        bpt::install_signal_handlers();
    }
    bpt::log::current_log_level = opts.log_level;
    return bpt::cli::dispatch_main(opts);
}

#if NEO_OS_IS_WINDOWS
#include <windows.h>
std::string wstr_to_u8str(std::wstring_view in) {
    if (in.empty()) {
        return "";
    }
    auto req_size = ::WideCharToMultiByte(CP_UTF8,
                                          0,
                                          in.data(),
                                          static_cast<int>(in.size()),
                                          nullptr,
                                          0,
                                          nullptr,
                                          nullptr);
    neo_assert(invariant,
               req_size > 0,
               "Failed to convert given unicode string for main() argv",
               req_size,
               std::system_category().message(::GetLastError()),
               ::GetLastError());
    std::string ret;
    ret.resize(req_size);
    ::WideCharToMultiByte(CP_UTF8,
                          0,
                          in.data(),
                          static_cast<int>(in.size()),
                          ret.data(),
                          static_cast<int>(ret.size()),
                          nullptr,
                          nullptr);
    return ret;
}

int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> u8_argv;
    for (int i = 0; i < argc; ++i) {
        u8_argv.emplace_back(wstr_to_u8str(argv[i]));
    }
    return main_fn(u8_argv[0], {u8_argv.cbegin() + 1, u8_argv.cend()});
}
#else
int main(int argc, char** argv) { return main_fn(argv[0], {argv + 1, argv + argc}); }
#endif
