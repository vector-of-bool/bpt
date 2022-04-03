#include "./dispatch_main.hpp"

#include "./error_handler.hpp"
#include "./options.hpp"

#include <bpt/util/paths.hpp>
#include <bpt/util/result.hpp>

using namespace bpt;

namespace bpt::cli {

namespace cmd {
using command = int(const options&);

command build_deps;
command build;
command compile_file;
command install_yourself;
command pkg_create;
command pkg_search;
command pkg_prefetch;
command pkg_solve;
command repo_cmd;

}  // namespace cmd

int dispatch_main(const options& opts) noexcept {
    return bpt::handle_cli_errors([&] {
        BPT_E_SCOPE(opts.subcommand);
        switch (opts.subcommand) {
        case subcommand::build:
            return cmd::build(opts);
        case subcommand::pkg: {
            BPT_E_SCOPE(opts.pkg.subcommand);
            switch (opts.pkg.subcommand) {
            case pkg_subcommand::create:
                return cmd::pkg_create(opts);
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
            BPT_E_SCOPE(opts.repo.subcommand);
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

}  // namespace bpt::cli
