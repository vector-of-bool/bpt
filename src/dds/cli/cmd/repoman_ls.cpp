#include "../options.hpp"

#include <dds/repoman/repoman.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <fmt/ostream.h>

#include <iostream>

namespace dds::cli::cmd {

static int _repoman_ls(const options& opts) {
    auto repo = repo_manager::open(opts.repoman.repo_dir);
    for (auto pkg_id : repo.all_packages()) {
        std::cout << pkg_id.to_string() << '\n';
    }
    return 0;
}

int repoman_ls(const options& opts) {
    return boost::leaf::try_catch(  //
        [&] {
            try {
                return _repoman_ls(opts);
            } catch (...) {
                dds::capture_exception();
            }
        },
        [](dds::e_system_error_exc e, dds::e_open_repo_db db) {
            dds_log(error, "Error while opening repository database {}: {}", db.path, e.message);
            return 1;
        });
}

}  // namespace dds::cli::cmd
