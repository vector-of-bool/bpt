#include "../options.hpp"

#include <dds/pkg/cache.hpp>
#include <dds/sdist/dist.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <fansi/styled.hpp>
#include <json5/parse_data.hpp>
#include <neo/assert.hpp>
#include <neo/url/parse.hpp>

#include <iostream>
#include <string_view>

using namespace fansi::literals;

namespace dds::cli::cmd {

struct e_importing {
    std::string value;
};

static int _pkg_import(const options& opts) {
    return pkg_cache::with_cache(  //
        opts.pkg_cache_dir.value_or(pkg_cache::default_local_path()),
        pkg_cache_flags::write_lock | pkg_cache_flags::create_if_absent,
        [&](auto repo) {
            // Lambda to import an sdist object
            auto import_sdist
                = [&](const sdist& sd) { repo.import_sdist(sd, dds::if_exists(opts.if_exists)); };

            for (std::string_view sdist_where : opts.pkg.import.items) {
                DDS_E_SCOPE(e_importing{std::string(sdist_where)});
                neo_assertion_breadcrumbs("Importing sdist", sdist_where);
                if (sdist_where.starts_with("http://") || sdist_where.starts_with("https://")) {
                    auto tmp_sd = download_expand_sdist_targz(sdist_where);
                    import_sdist(tmp_sd.sdist);
                } else if (fs::is_directory(sdist_where)) {
                    auto sd = sdist::from_directory(sdist_where);
                    import_sdist(sd);
                } else {
                    auto tmp_sd = expand_sdist_targz(sdist_where);
                    import_sdist(tmp_sd.sdist);
                }
            }
            if (opts.pkg.import.from_stdin) {
                auto tmp_sd = dds::expand_sdist_from_istream(std::cin, "<stdin>");
                repo.import_sdist(tmp_sd.sdist, dds::if_exists(opts.if_exists));
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
        },
        [](e_system_error_exc err, e_importing what) {
            dds_log(
                error,
                "Error while importing source distribution from [.bold.red[{}]]: .br.yellow[{}]"_styled,
                what.value,
                err.message);
            return 1;
        });
}
}  // namespace dds::cli::cmd
