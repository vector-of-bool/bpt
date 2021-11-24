#include "../options.hpp"

#include <dds/repoman/repoman.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf.hpp>
#include <fmt/ostream.h>
#include <neo/sqlite3/error.hpp>

namespace dds::cli::cmd {

static int _repoman_init(const options& opts) {
    auto repo = repo_manager::create(opts.repoman.repo_dir, opts.repoman.init.name);
    dds_log(info, "Created new repository '{}' in {}", repo.name(), repo.root());
    return 0;
}

int repoman_init(const options& opts) {
    return boost::leaf::try_catch(  //
        [&] { return _repoman_init(opts); },
        [](boost::leaf::catch_<neo::sqlite3::error> e,
           dds::e_init_repo                         init,
           dds::e_init_repo_db                      init_db) {
            dds_log(error,
                    "SQLite error while initializing repository in [{}] (SQlite database {}): {}",
                    init.path,
                    init_db.path,
                    e.matched.what());
            return 1;
        },
        [](const std::system_error& e, dds::e_open_repo_db db) {
            dds_log(error,
                    "Error while opening repository database {}: {}",
                    db.path,
                    e.code().message());
            return 1;
        },
        [](boost::leaf::catch_<neo::sqlite3::error> e, dds::e_init_repo init) {
            dds_log(error,
                    "SQLite error while initializing repository in [{}]: {}",
                    init.path,
                    e.matched.what());
            return 1;
        });
}

}  // namespace dds::cli::cmd
