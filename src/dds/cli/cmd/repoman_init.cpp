#include "../options.hpp"

#include <dds/repoman/repoman.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <fmt/ostream.h>

namespace dds::cli::cmd {

static int _repoman_init(const options& opts) {
    auto repo = repo_manager::create(opts.repoman.repo_dir, opts.repoman.init.name);
    dds_log(info, "Created new repository '{}' in {}", repo.name(), repo.root());
    return 0;
}

int repoman_init(const options& opts) {
    return boost::leaf::try_catch(  //
        [&] {
            try {
                return _repoman_init(opts);
            } catch (...) {
                dds::capture_exception();
            }
        },
        [](dds::e_sqlite3_error_exc e, dds::e_init_repo init, dds::e_init_repo_db init_db) {
            dds_log(error,
                    "SQLite error while initializing repository in [{}] (SQlite database {}): {}",
                    init.path,
                    init_db.path,
                    e.message);
            return 1;
        },
        [](dds::e_system_error_exc e, dds::e_open_repo_db db) {
            dds_log(error, "Error while opening repository database {}: {}", db.path, e.message);
            return 1;
        },
        [](dds::e_sqlite3_error_exc e, dds::e_init_repo init) {
            dds_log(error,
                    "SQLite error while initializing repository in [{}]: {}",
                    init.path,
                    e.message);
            return 1;
        });
}

}  // namespace dds::cli::cmd
