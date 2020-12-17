#include "./dispatch_main.hpp"

#include "./error_handler.hpp"
#include "./options.hpp"

#include <dds/catalog/catalog.hpp>
#include <dds/remote/remote.hpp>
#include <dds/util/paths.hpp>
#include <dds/util/result.hpp>

using namespace dds;

namespace dds::cli {

namespace cmd {
using command = int(const options&);

command build_deps;
command build;
command compile_file;
command pkg_get;
command pkg_import;
command pkg_ls;
command pkg_repo_add;
command pkg_repo_update;
command repoman_import;
command repoman_init;
command repoman_ls;
command repoman_remove;
command sdist_create;

}  // namespace cmd

int dispatch_main(const options& opts) noexcept {
    dds::log::current_log_level = opts.log_level;
    return dds::handle_cli_errors([&] {
        switch (opts.subcommand) {
        case subcommand::build:
            return cmd::build(opts);
        case subcommand::sdist:
            switch (opts.sdist.subcommand) {
            case sdist_subcommand::create:
                return cmd::sdist_create(opts);
            case sdist_subcommand::_none_:;
            }
            neo::unreachable();
        case subcommand::pkg:
            switch (opts.pkg.subcommand) {
            case pkg_subcommand::ls:
                return cmd::pkg_ls(opts);
            case pkg_subcommand::get:
                return cmd::pkg_get(opts);
            case pkg_subcommand::import:
                return cmd::pkg_import(opts);
            case pkg_subcommand::repo:
                switch (opts.pkg.repo.subcommand) {
                case cli_pkg_repo_subcommand::add:
                    return cmd::pkg_repo_add(opts);
                case cli_pkg_repo_subcommand::update:
                    return cmd::pkg_repo_update(opts);
                case cli_pkg_repo_subcommand::_none_:;
                }
                neo::unreachable();
            case pkg_subcommand::_none_:;
            }
            neo::unreachable();
        case subcommand::repoman:
            switch (opts.repoman.subcommand) {
            case repoman_subcommand::import:
                return cmd::repoman_import(opts);
            case repoman_subcommand::init:
                return cmd::repoman_init(opts);
            case repoman_subcommand::remove:
                return cmd::repoman_remove(opts);
            case repoman_subcommand::ls:
                return cmd::repoman_ls(opts);
            case repoman_subcommand::_none_:;
            }
            neo::unreachable();
        case subcommand::compile_file:
            return cmd::compile_file(opts);
        case subcommand::build_deps:
            return cmd::build_deps(opts);
        case subcommand::_none_:;
        }
        neo::unreachable();
        return 6;
    });
}

}  // namespace dds::cli
