#include "../options.hpp"

#include <dds/error/handle.hpp>
#include <dds/error/result.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/repoman/repoman.hpp>

#include <boost/leaf.hpp>
#include <fansi/styled.hpp>
#include <fmt/ostream.h>
#include <neo/sqlite3/error.hpp>

using namespace fansi::literals;

namespace dds::cli::cmd {

static int _repoman_import(const options& opts) {
    auto repo = repo_manager::open(opts.repoman.repo_dir);
    for (auto pkg : opts.repoman.import.files) {
        dds_leaf_try { repo.import_targz(pkg); }
        dds_leaf_catch(neo::sqlite3::constraint_unique_error, dds::pkg_id pkid) {
            if (opts.if_exists == if_exists::ignore) {
                dds_log(info,
                        "Ignoring already-imported package .cyan[{}]"_styled,
                        pkid.to_string());
            } else if (opts.if_exists == if_exists::replace) {
                dds_log(
                    info,
                    "Replacing previously-imported package .yellow[{}] with new package."_styled,
                    pkid.to_string());
                repo.delete_package(pkid);
                repo.import_targz(pkg);
            } else {
                // Fail
                throw;
            }
        };
    }
    return 0;
}

int repoman_import(const options& opts) {
    return dds_leaf_try { return _repoman_import(opts); }
    dds_leaf_catch(neo::sqlite3::constraint_unique_error,
                   e_repo_import_targz tgz,
                   dds::pkg_id         pkid) {
        dds_log(error,
                "Package {} (from {}) is already present in the repository",
                pkid.to_string(),
                tgz.path);
        return 1;
    }
    dds_leaf_catch(const std::system_error& e, dds::e_repo_import_targz tgz) {
        dds_log(error, "Failed to import file {}: {}", tgz.path, e.code().message());
        return 1;
    }
    dds_leaf_catch(const std::runtime_error& e, dds::e_repo_import_targz tgz) {
        dds_log(error, "Unknown error while importing file {}: {}", tgz.path, e.what());
        return 1;
    }
    dds_leaf_catch(catch_<neo::sqlite3::error> e, dds::e_repo_import_targz tgz) {
        dds_log(error,
                "Database error while importing tar file {}: {}",
                tgz.path,
                e.matched.what());
        return 1;
    }
    dds_leaf_catch(const std::system_error& e, dds::e_open_repo_db db) {
        dds_log(error,
                "Error while opening repository database {}: {}",
                db.path,
                e.code().message());
        return 1;
    };
}

}  // namespace dds::cli::cmd
