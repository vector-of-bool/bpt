#include "../options.hpp"

#include <dds/http/session.hpp>
#include <dds/repo/repo.hpp>
#include <dds/source/dist.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <json5/parse_data.hpp>
#include <neo/assert.hpp>
#include <neo/url/parse.hpp>

#include <iostream>
#include <string_view>

namespace dds::cli::cmd {
static int _pkg_import(const options& opts) {
    return repository::with_repository(  //
        opts.pkg_cache_dir.value_or(repository::default_local_path()),
        repo_flags::write_lock | repo_flags::create_if_absent,
        [&](auto repo) {
            for (std::string_view tgz_where : opts.pkg.import.items) {
                neo_assertion_breadcrumbs("Importing sdist", tgz_where);
                auto tmp_sd
                    = (tgz_where.starts_with("http://") || tgz_where.starts_with("https://"))
                    ? download_expand_sdist_targz(tgz_where)
                    : expand_sdist_targz(tgz_where);
                neo_assertion_breadcrumbs("Importing from temporary directory",
                                          tmp_sd.tmpdir.path());
                repo.add_sdist(tmp_sd.sdist, dds::if_exists(opts.if_exists));
            }
            if (opts.pkg.import.from_stdin) {
                auto tmp_sd = dds::expand_sdist_from_istream(std::cin, "<stdin>");
                repo.add_sdist(tmp_sd.sdist, dds::if_exists(opts.if_exists));
            }
            return 0;
        });
}

int pkg_import(const options& opts) {
    return boost::leaf::try_catch(
        [&] {
            try {
                return _pkg_import(opts);
            } catch (...) {
                dds::capture_exception();
            }
        },
        [&](const json5::parse_error& e) {
            dds_log(error, "Error parsing JSON in package archive: {}", e.what());
            return 1;
        },
        [](dds::e_sqlite3_error_exc e) {
            dds_log(error, "Unexpected database error: {}", e.message);
            return 1;
        });
}
}  // namespace dds::cli::cmd
