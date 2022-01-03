#include "../options.hpp"

#include <dds/crs/error.hpp>
#include <dds/crs/repo.hpp>
#include <dds/error/errors.hpp>
#include <dds/error/handle.hpp>
#include <dds/error/marker.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/project/error.hpp>
#include <dds/util/db/migrate.hpp>
#include <dds/util/fs/io.hpp>
#include <dds/util/fs/shutil.hpp>
#include <dds/util/json5/error.hpp>

#include <boost/leaf/pred.hpp>
#include <fansi/styled.hpp>
#include <neo/event.hpp>
#include <neo/sqlite3/error.hpp>

using namespace fansi::literals;

namespace dds::cli::cmd {

namespace {

void _import_file(dds::crs::repository& repo, path_ref dirpath) { repo.import_dir(dirpath); }

int _repo_import(const options& opts) {
    auto repo = dds::crs::repository::open_existing(opts.repoman.repo_dir);
    NEO_SUBSCRIBE(dds::crs::ev_repo_imported_package imported) {
        dds_log(info,
                "[{}]: Imported .bold.cyan[{}@{}] from [.br.cyan[{}]]"_styled,
                imported.into_repo.name(),
                imported.pkg_meta.name.str,
                imported.pkg_meta.version.to_string(),
                imported.from_path.string());
    };
    for (auto& path : opts.repoman.import.files) {
        dds_leaf_try {
            dds_log(debug, "Importing CRS package from [.br.cyan[{}]]"_styled, path.string());
            _import_file(repo, path);
        }
        dds_leaf_catch(dds::crs::e_repo_import_pkg_already_present,
                       dds::crs::e_repo_importing_package meta) {
            switch (opts.if_exists) {
            case if_exists::ignore:
                dds_log(info,
                        "Ignoring existing package .br.cyan[{}@{}] (from .br.white[{}])"_styled,
                        meta.value.name.str,
                        meta.value.version.to_string(),
                        path.string());
                return;
            case if_exists::fail:
                throw;
            case if_exists::replace:
                dds_log(info,
                        "Replacing existing package .br.yellow[{}@{}]"_styled,
                        meta.value.name.str,
                        meta.value.version.to_string());
                repo.remove_pkg(meta.value);
                _import_file(repo, path);
                return;
            }
        };
    }
    return 0;
}

}  // namespace

int repo_import(const options& opts) {
    return dds_leaf_try { return _repo_import(opts); }
    dds_leaf_catch(dds::crs::e_repo_import_pkg_already_present,
                   dds::crs::e_repo_importing_package meta,
                   dds::crs::e_repo_importing_dir     from_dir) {
        dds_log(
            error,
            "Refusing to overwrite existing package .br.yellow[{}@{}] (Importing from [.br.yellow[{}]])"_styled,
            meta.value.name.str,
            meta.value.version.to_string(),
            from_dir.value.string());
        write_error_marker("repo-import-pkg-already-exists");
        return 1;
    }
    dds_leaf_catch(dds::crs::e_repo_importing_dir crs_dir,
                   std::error_code                ec,
                   boost::leaf::match<std::error_code, boost::leaf::category<neo::sqlite3::errc>>,
                   e_sqlite3_error const* sqlite_error) {
        dds_log(error,
                "SQLite error while importing [.br.yellow[{}]]"_styled,
                crs_dir.value.string());
        dds_log(error, "  .br.red[{}]"_styled, ec.message());
        if (sqlite_error) {
            dds_log(error, "    .br.red[{}]"_styled, sqlite_error->value);
        }
        dds_log(error, "(It's possible that the database is invalid or corrupted)");
        write_error_marker("repo-import-db-error");
        return 1;
    }
    dds_leaf_catch(dds::e_json_parse_error        parse_error,
                   dds::crs::e_repo_importing_dir crs_dir,
                   dds::crs::e_pkg_json_path      pkg_json_path) {
        dds_log(error, "Error while importing [.br.yellow[{}]]"_styled, crs_dir.value.string());
        dds_log(error,
                "  JSON parse error in [.br.yellow[{}]]:"_styled,
                pkg_json_path.value.string());
        dds_log(error, "    .br.red[{}]"_styled, parse_error.value);
        write_error_marker("repo-import-invalid-crs-json-parse-error");
        return 1;
    }
    dds_leaf_catch(dds::crs::e_invalid_meta_data  error,
                   dds::crs::e_repo_importing_dir crs_dir,
                   dds::crs::e_pkg_json_path      pkg_json_path) {
        dds_log(error, "Error while importing [.br.yellow[{}]]"_styled, crs_dir.value.string());
        dds_log(error,
                "CRS data in [.br.yellow[{}]] is invalid: .br.red[{}]"_styled,
                pkg_json_path.value.string(),
                error.value);
        write_error_marker("repo-import-invalid-crs-json");
        return 1;
    }
    dds_leaf_catch(dds::crs::e_invalid_meta_data  error,
                   dds::crs::e_repo_importing_dir proj_dir,
                   dds::e_open_project,
                   dds::e_parse_project_manifest_path proj_json_path) {
        dds_log(error, "Error while importing [.br.yellow[{}]]"_styled, proj_dir.value.string());
        dds_log(error,
                "Project data in [.br.yellow[{}]] is invalid: .br.red[{}]"_styled,
                proj_json_path.value.string(),
                error.value);
        write_error_marker("repo-import-invalid-proj-json");
        return 1;
    }
    dds_leaf_catch(user_error<errc::invalid_pkg_filesystem> const& exc,
                   crs::e_repo_importing_dir                       crs_dir) {
        dds_log(error,
                "Error while importing [.br.yellow[{}]]: .br.red[{}]"_styled,
                crs_dir.value.string(),
                exc.what());
        write_error_marker("repo-import-noent");
        return 1;
    }
    dds_leaf_catch(dds::crs::e_repo_importing_dir crs_dir,
                   dds::e_read_file_path const*   read_file,
                   dds::e_copy_file const*        copy_file,
                   std::error_code                ec) {
        dds_log(error, "Error while importing [.br.red[{}]]:"_styled, crs_dir.value.string());
        if (read_file) {
            dds_log(error,
                    " Error reading file [.br.yellow[{}]]:"_styled,
                    read_file->value.string(),
                    ec.message());
        } else if (copy_file) {
            dds_log(error,
                    " Error copying file [.br.yellow[{}]] to [.br.yellow[{}]]:"_styled,
                    copy_file->source.string(),
                    copy_file->dest.string());
        }
        dds_log(error, "  .br.red[{}]"_styled, ec.message());
        write_error_marker("repo-import-noent");
        return 1;
    };
}

}  // namespace dds::cli::cmd
