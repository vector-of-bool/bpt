#include "../options.hpp"

#include <dds/error/errors.hpp>
#include <dds/pkg/get/get.hpp>
#include <dds/pkg/listing.hpp>
#include <dds/repoman/repoman.hpp>
#include <dds/util/http/error.hpp>
#include <dds/util/http/pool.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf/pred.hpp>
#include <fmt/ostream.h>
#include <neo/sqlite3/error.hpp>

namespace dds::cli::cmd {

static int _repoman_add(const options& opts) {
    auto rpkg       = any_remote_pkg::from_url(neo::url::parse(opts.repoman.add.url_str));
    auto temp_sdist = get_package_sdist(rpkg);

    dds::pkg_listing add_info{
        .ident       = temp_sdist.sdist.manifest.id,
        .deps        = temp_sdist.sdist.manifest.dependencies,
        .description = opts.repoman.add.description,
        .remote_pkg  = rpkg,
    };

    auto repo = repo_manager::open(opts.repoman.repo_dir);
    repo.add_pkg(add_info, opts.repoman.add.url_str);
    return 0;
}

int repoman_add(const options& opts) {
    return boost::leaf::try_catch(  //
        [&] { return _repoman_add(opts); },
        [](boost::leaf::catch_<neo::sqlite3::constraint_unique_error>, dds::pkg_id pkid) {
            dds_log(error, "Package {} is already present in the repository", pkid.to_string());
            write_error_marker("dup-pkg-add");
            return 1;
        },
        [](http_status_error, http_response_info resp, neo::url url) {
            dds_log(error,
                    "Error resulted from HTTP request [{}]: {} {}",
                    url.to_string(),
                    resp.status,
                    resp.status_message);
            return 1;
        },
        [](dds::user_error<errc::invalid_remote_url> e, neo::url url) -> int {
            dds_log(error, "Invalid URL '{}': {}", url.to_string(), e.what());
            write_error_marker("repoman-add-invalid-pkg-url");
            throw;
        },
        [](boost::leaf::catch_<neo::sqlite3::error> e, dds::e_repo_import_targz tgz) {
            dds_log(error,
                    "Database error while importing tar file {}: {}",
                    tgz.path,
                    e.matched.what());
            return 1;
        },
        [](const std::system_error& e, dds::e_open_repo_db db) {
            dds_log(error,
                    "Error while opening repository database {}: {}",
                    db.path,
                    e.code().message());
            return 1;
        });
}

}  // namespace dds::cli::cmd
