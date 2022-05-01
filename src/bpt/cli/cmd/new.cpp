#include "../options.hpp"

#include <bpt/error/try_catch.hpp>
#include <bpt/util/fs/io.hpp>
#include <bpt/util/fs/path.hpp>
#include <bpt/util/name.hpp>
#include <bpt/util/signal.hpp>

#include <fansi/styled.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <neo/tl.hpp>
#include <neo/utility.hpp>

#include <iostream>

using namespace bpt;
using namespace fansi::literals;

namespace bpt::cli::cmd {

namespace {

std::string get_argument(std::string_view                  prompt,
                         std::optional<std::string> const& opt,
                         std::string_view                  default_) {
    if (opt.has_value()) {
        return opt.value();
    }
    if (default_.empty()) {
        fmt::print(".magenta[{}]: "_styled, prompt, default_);
    } else {
        fmt::print(".magenta[{}] [.blue[{}]]: "_styled, prompt, default_);
    }
    std::string ret;
    std::getline(std::cin, ret);
    if (!std::cin.good()) {
        throw bpt::user_cancelled{};
    }
    if (ret.empty()) {
        return std::string(default_);
    }
    return ret;
}

std::string to_ident(std::string_view given) {
    std::string ret;
    std::ranges::replace_copy_if(given, std::back_inserter(ret), NEO_TL(!std::isalnum(_1)), '_');
    if (!ret.empty() && std::isdigit(ret.front())) {
        ret.insert(ret.begin(), '_');
    }
    return ret;
}

const std::string_view BPT_YAML_TEMPLATE = R"(# Project '{0}' created by 'bpt new'
name: {0}
version: 0.1.0

## Use 'dependencies' to declare dependencies on external libraries
# dependencies:
#   - fmt@8.1.1

## Use 'test-dependencies' to declare dependencies on external libraries to be
## used by this project's tests
# test-dependencies:
#   - catch2@2.13.9 using main

## The relative filepath to the project's README file
readme: README.md

## Provide a very brief description of the project
description: |
  The is the {0} project. Hello!

## Point to the project's homepage on the web
# homepage: example.com

## The source-control repository of the project
# repository: example.com/{0}/source

## A URL to the documentation for the project
# documentation: example.com/{0}/docs

## Specify an SPX License ID for the project
# license:

## List the authors of the project
# authors:
#   - My Name <my.email@example.org>
#   - Other Name <other.email@example.org>
)";

const std::string_view README_MD_TEMPLATE = R"(<!-- README.md created by 'bpt new' -->
# `{0}` - A Great New Project

Add introductory information about your project to this file, which will likely
be the first thing that potential new users will see.
)";

}  // namespace

int new_cmd(const options& opts) {
    auto        given_name = opts.new_.name;
    std::string name;
    while (name.empty()) {
        bpt_leaf_try {
            name = bpt::name::from_string(get_argument("New project name", given_name, ""))->str;
        }
        bpt_leaf_catch(bpt::e_name_str given, bpt::invalid_name_reason why) {
            given_name.reset();
            fmt::print(std::cerr,
                       "Invalid name string \".bold.red[{}]\": .bold.yellow[{}]\n"_styled,
                       given.value,
                       bpt::invalid_name_reason_str(why));
        };
    }

    std::string dest;
    while (dest.empty()) {
        auto default_dir = bpt::resolve_path_weak(fs::absolute(name));
        dest = get_argument("Project directory", opts.new_.directory, default_dir.string());
        if (dest.empty()) {
            continue;
        }
        dest = bpt::resolve_path_weak(fs::absolute(dest)).string();
        if (fs::exists(dest)) {
            if (!fs::is_directory(dest)) {
                fmt::print(std::cerr,
                           "Path [.bold.red[{}]] names an existing non-directory file\n"_styled,
                           dest);
                dest.clear();
                continue;
            }
            if (fs::directory_iterator{dest} != fs::directory_iterator{}) {
                fmt::print(std::cerr,
                           "Path [.bold.red[{}]] is an existing non-empty directory\n"_styled,
                           dest);
                dest.clear();
                continue;
            }
        }
    }

    bool split_dir = false;
    if (opts.new_.split_src_include.has_value()) {
        split_dir = *opts.new_.split_src_include;
    } else {
        while (true) {
            auto got = get_argument(
                "Split headers and sources into [include/] and [src/] directories? [y/N]",
                std::nullopt,
                "");
            if (got == neo::oper::any_of("n", "N", "no", "")) {
                split_dir = false;
                break;
            } else if (got == neo::oper::any_of("y", "Y", "yes")) {
                split_dir = true;
                break;
            } else {
                fmt::print(std::cerr, "Enter one of '.bold.cyan[y]' or '.bold.cyan[n]'"_styled);
            }
        }
    }

    fs::path project_dir = dest;
    fs::create_directories(project_dir);
    bpt::write_file(project_dir / "bpt.yaml", fmt::format(BPT_YAML_TEMPLATE, name));

    fs::path header_dir = split_dir ? (project_dir / "include") : (project_dir / "src");
    fs::path src_dir    = project_dir / "src";
    fs::create_directories(header_dir / name);
    fs::create_directories(src_dir / name);
    bpt::write_file(src_dir / name / fmt::format("{}.cpp", name),
                    fmt::format("#include <{0}/{0}.hpp>\n\n"
                                "int {1}::the_answer() noexcept {{\n"
                                "  return 42;\n"
                                "}}\n",
                                name,
                                to_ident(name)));

    bpt::write_file(header_dir / name / fmt::format("{}.hpp", name),
                    fmt::format("#pragma once\n\n"
                                "namespace {0} {{\n\n"
                                "// Calculate the answer\n"
                                "int the_answer() noexcept;\n\n"
                                "}}\n",
                                to_ident(name)));

    bpt::write_file(project_dir / "README.md", fmt::format(README_MD_TEMPLATE, name));
    fmt::print("New project files written to [.bold.cyan[{}]]\n"_styled, dest);
    return 0;
}

}  // namespace bpt::cli::cmd
