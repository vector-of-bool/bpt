#include <dds/cli/dispatch_main.hpp>
#include <dds/cli/options.hpp>
#include <dds/util/log.hpp>
#include <dds/util/signal.hpp>

#include <debate/debate.hpp>
#include <debate/enum.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <fmt/ostream.h>

#include <filesystem>
#include <iostream>

int main_fn(std::string_view program_name, const std::vector<std::string>& argv) {
    dds::log::init_logger();
    dds::install_signal_handlers();

    dds::cli::options       opts;
    debate::argument_parser parser;
    opts.setup_parser(parser);

    auto result = boost::leaf::try_catch(
        [&]() -> std::optional<int> {
            parser.parse_argv(argv);
            return {};
        },
        [&](debate::help_request, debate::e_argument_parser p) {
            std::cout << p.parser.help_string(program_name);
            return 0;
        },
        [&](debate::unrecognized_argument,
            debate::e_argument_parser p,
            debate::e_arg_spelling    arg) {
            std::cerr << p.parser.usage_string(program_name) << '\n';
            if (p.parser.subparsers()) {
                fmt::print(std::cerr, "Unrecognized argument/subcommand: \"{}\"\n", arg.spelling);
            } else {
                fmt::print(std::cerr, "Unrecognized argument: \"{}\"\n", arg.spelling);
            }
            return 2;
        },
        [&](debate::invalid_arguments,
            debate::e_argument          arg,
            debate::e_argument_parser   p,
            debate::e_arg_spelling      spell,
            debate::e_invalid_arg_value val) {
            std::cerr << p.parser.usage_string(program_name) << '\n';
            fmt::print(std::cerr,
                       "Invalid {} value '{}' given for '{}'\n",
                       arg.argument.valname,
                       val.given,
                       spell.spelling);
            return 2;
        },
        [&](debate::invalid_arguments,
            debate::e_argument_parser p,
            debate::e_arg_spelling    spell,
            debate::e_argument        arg,
            debate::e_wrong_val_num   given) {
            std::cerr << p.parser.usage_string(program_name) << '\n';
            if (arg.argument.nargs == 0) {
                fmt::print(std::cerr,
                           "Argument '{}' does not expect any values, but was given one\n",
                           spell.spelling);
            } else if (arg.argument.nargs == 1 && given.n_given == 0) {
                fmt::print(std::cerr,
                           "Argument '{}' expected to be given a value, but received none\n",
                           spell.spelling);
            } else {
                fmt::print(
                    std::cerr,
                    "Wrong number of arguments provided for '{}': Expected {}, but only got {}\n",
                    spell.spelling,
                    arg.argument.nargs,
                    given.n_given);
            }
            return 2;
        },
        [&](debate::missing_required, debate::e_argument_parser p, debate::e_argument arg) {
            fmt::print(std::cerr,
                       "{}\nMissing required argument '{}'\n",
                       p.parser.usage_string(program_name),
                       arg.argument.preferred_spelling());
            return 2;
        },
        [&](debate::invalid_repitition, debate::e_argument_parser p, debate::e_arg_spelling sp) {
            fmt::print(std::cerr << "{}\nArgument '{}' cannot be provided more than once\n",
                       p.parser.usage_string(program_name),
                       sp.spelling);
            return 2;
        },
        [&](debate::missing_required err, debate::e_argument_parser p) {
            fmt::print(std::cerr,
                       "{}\nError: {}\n",
                       p.parser.usage_string(program_name),
                       err.what());
            return 2;
        });
    if (result) {
        // Non-null result from argument parsing, return that value immediately.
        return *result;
    }
    return dds::cli::dispatch_main(opts);
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
    ::setlocale(LC_ALL, ".utf8");
    for (int i = 0; i < argc; ++i) {
        u8_argv.emplace_back(wstr_to_u8str(argv[i]));
    }
    return main_fn(u8_argv[0], {u8_argv.cbegin() + 1, u8_argv.cend()});
}
#else
int main(int argc, char** argv) { return main_fn(argv[0], {argv + 1, argv + argc}); }
#endif
