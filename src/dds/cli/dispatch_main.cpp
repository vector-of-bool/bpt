#include "./dispatch_main.hpp"

#include "./error_handler.hpp"
#include "./options.hpp"

#include <dds/util/paths.hpp>
#include <dds/util/result.hpp>

using namespace dds;

namespace dds::cli {

namespace cmd {
using command = int(const options&);

command build_deps;
command build;
command compile_file;
command install_yourself;
command pkg_create;
command pkg_get;
command pkg_import;
command pkg_ls;
command pkg_repo_add;
command pkg_repo_update;
command pkg_repo_ls;
command pkg_repo_remove;
command pkg_search;
command pkg_prefetch;
command pkg_solve;
command repo_cmd;

}  // namespace cmd

int dispatch_main(const options& opts) noexcept {
    return dds::handle_cli_errors([&] {
        DDS_E_SCOPE(opts.subcommand);
        switch (opts.subcommand) {
        case subcommand::build:
            return cmd::build(opts);
        case subcommand::pkg: {
            DDS_E_SCOPE(opts.pkg.subcommand);
            switch (opts.pkg.subcommand) {
            case pkg_subcommand::ls:
                return cmd::pkg_ls(opts);
            case pkg_subcommand::create:
                return cmd::pkg_create(opts);
            case pkg_subcommand::get:
                return cmd::pkg_get(opts);
            case pkg_subcommand::import:
                return cmd::pkg_import(opts);
            case pkg_subcommand::repo: {
                DDS_E_SCOPE(opts.pkg.repo.subcommand);
                switch (opts.pkg.repo.subcommand) {
                case pkg_repo_subcommand::add:
                    return cmd::pkg_repo_add(opts);
                case pkg_repo_subcommand::update:
                    return cmd::pkg_repo_update(opts);
                case pkg_repo_subcommand::ls:
                    return cmd::pkg_repo_ls(opts);
                case pkg_repo_subcommand::remove:
                    return cmd::pkg_repo_remove(opts);
                case pkg_repo_subcommand::_none_:;
                }
                neo::unreachable();
            }
            case pkg_subcommand::search:
                return cmd::pkg_search(opts);
            case pkg_subcommand::prefetch:
                return cmd::pkg_prefetch(opts);
            case pkg_subcommand::solve:
                return cmd::pkg_solve(opts);
            case pkg_subcommand::_none_:;
            }
            neo::unreachable();
        }
        case subcommand::repo: {
            DDS_E_SCOPE(opts.repo.subcommand);
            return cmd::repo_cmd(opts);
        }
        case subcommand::compile_file:
            return cmd::compile_file(opts);
        case subcommand::build_deps:
            return cmd::build_deps(opts);
        case subcommand::install_yourself:
            return cmd::install_yourself(opts);
        case subcommand::_none_:;
        }
        neo::unreachable();
        return 6;
    });
}

}  // namespace dds::cli
