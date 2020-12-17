#include "../options.hpp"

#include <dds/repoman/repoman.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <fmt/ostream.h>
#include <neo/sqlite3/error.hpp>

namespace dds::cli::cmd {

static int _repoman_import(const options& opts) {
    auto repo = repo_manager::open(opts.repoman.repo_dir);
    for (auto pkg : opts.repoman.import.files) {
        repo.import_targz(pkg);
    }
    return 0;
}

int repoman_import(const options& opts) {
    return boost::leaf::try_catch(  //
        [&] {
            try {
                return _repoman_import(opts);
            } catch (...) {
                dds::capture_exception();
            }
        },
        [](dds::e_sqlite3_error_exc,
           boost::leaf::match<neo::sqlite3::errc, neo::sqlite3::errc::constraint_unique>,
           dds::e_repo_import_targz tgz,
           dds::package_id          pkg_id) {
            dds_log(error,
                    "Package {} (from {}) is already present in the repository",
                    pkg_id.to_string(),
                    tgz.path);
            return 1;
        },
        [](dds::e_system_error_exc e, dds::e_repo_import_targz tgz) {
            dds_log(error, "Failed to import file {}: {}", tgz.path, e.message);
            return 1;
        },
        [](const std::runtime_error& e, dds::e_repo_import_targz tgz) {
            dds_log(error, "Unknown error while importing file {}: {}", tgz.path, e.what());
            return 1;
        },
        [](dds::e_sqlite3_error_exc e, dds::e_repo_import_targz tgz) {
            dds_log(error, "Database error while importing tar file {}: {}", tgz.path, e.message);
            return 1;
        },
        [](dds::e_system_error_exc e, dds::e_open_repo_db db) {
            dds_log(error, "Error while opening repository database {}: {}", db.path, e.message);
            return 1;
        });
}

}  // namespace dds::cli::cmd
