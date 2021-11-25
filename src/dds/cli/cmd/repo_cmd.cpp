#include "../options.hpp"

#include <dds/crs/repo.hpp>
#include <dds/error/handle.hpp>
#include <dds/error/marker.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/util/db/migrate.hpp>

#include <fansi/styled.hpp>
#include <neo/assert.hpp>

using namespace dds;
using namespace fansi::literals;

namespace dds::cli::cmd {

using command = int(const options&);

command repo_init;
command repo_import;
command repo_ls;

int repo_cmd(const options& opts) {
    neo_assert(invariant, opts.subcommand == subcommand::repo, "Wrong subcommand for dispatch");
    return dds_leaf_try {
        switch (opts.repoman.subcommand) {
        case repoman_subcommand::init:
            return cmd::repo_init(opts);
        case repoman_subcommand::import:
            return cmd::repo_import(opts);
        case repoman_subcommand::ls:
            return cmd::repo_ls(opts);
        default:
        case repoman_subcommand::_none_:
            neo::unreachable();
        }
    }
    dds_leaf_catch(dds::crs::e_repo_open_path db_path, matchv<db_migration_errc::too_new>) {
        dds_log(
            error,
            "Repository [.br.yellow[{}]] is from a newer dds version. We don't know how to handle it."_styled,
            db_path.value.string());
        write_error_marker("repo-import-db-too-new");
        return 1;
    }
    dds_leaf_catch(dds::crs::e_repo_open_path repo_path, e_migration_error error) {
        dds_log(
            error,
            "Error while opening SQLite database for repostiory [.br.yellow[{}]]: .br.red[{}]"_styled,
            repo_path.value.string(),
            error.value);
        write_error_marker("repo-import-db-invalid");
        return 1;
    }
    dds_leaf_catch(dds::crs::e_repo_open_path, dds::e_db_open_path db_path, dds::e_db_open_ec ec) {
        dds_log(error,
                "Error opening repository database [.br.yellow[{}]]: {}"_styled,
                db_path.value,
                ec.value.message());
        write_error_marker("repo-import-repo-open-fails");
        return 1;
    };
}

}  // namespace dds::cli::cmd