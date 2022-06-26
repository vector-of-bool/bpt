#include "../options.hpp"

#include <bpt/crs/error.hpp>
#include <bpt/crs/repo.hpp>
#include <bpt/error/errors.hpp>
#include <bpt/error/handle.hpp>
#include <bpt/error/marker.hpp>
#include <bpt/error/try_catch.hpp>
#include <bpt/project/error.hpp>
#include <bpt/util/db/migrate.hpp>
#include <bpt/util/fs/io.hpp>
#include <bpt/util/fs/shutil.hpp>
#include <bpt/util/json5/error.hpp>

#include <boost/leaf/pred.hpp>
#include <fansi/styled.hpp>
#include <neo/event.hpp>
#include <neo/sqlite3/error.hpp>

using namespace fansi::literals;

namespace bpt::cli::cmd {

namespace {

void _import_file(bpt::crs::repository& repo, path_ref dirpath) { repo.import_dir(dirpath); }

int _repo_import(const options& opts) {
    auto repo = bpt::crs::repository::open_existing(opts.repo.repo_dir);
    NEO_SUBSCRIBE(bpt::crs::ev_repo_imported_package imported) {
        bpt_log(info,
                "[{}]: Imported .bold.cyan[{}] from [.br.cyan[{}]]"_styled,
                imported.into_repo.name(),
                imported.pkg_meta.id.to_string(),
                imported.from_path.string());
    };
    for (auto& path : opts.repo.import.files) {
        bpt_leaf_try {
            bpt_log(debug, "Importing CRS package from [.br.cyan[{}]]"_styled, path.string());
            _import_file(repo, path);
        }
        bpt_leaf_catch(bpt::crs::e_repo_import_pkg_already_present,
                       bpt::crs::e_repo_importing_package meta) {
            switch (opts.if_exists) {
            case if_exists::ignore:
                bpt_log(info,
                        "Ignoring existing package .br.cyan[{}] (from .br.white[{}])"_styled,
                        meta.value.id.to_string(),
                        path.string());
                return;
            case if_exists::fail:
                throw;
            case if_exists::replace:
                bpt_log(info,
                        "Replacing existing package .br.yellow[{}]"_styled,
                        meta.value.id.to_string());
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
    return bpt_leaf_try { return _repo_import(opts); }
    bpt_leaf_catch(bpt::crs::e_repo_import_pkg_already_present,
                   bpt::crs::e_repo_importing_package meta,
                   bpt::crs::e_repo_importing_dir     from_dir) {
        bpt_log(
            error,
            "Refusing to overwrite existing package .br.yellow[{}] (Importing from [.br.yellow[{}]])"_styled,
            meta.value.id.to_string(),
            from_dir.value.string());
        write_error_marker("repo-import-pkg-already-exists");
        return 1;
    }
    bpt_leaf_catch(bpt::crs::e_repo_importing_dir crs_dir,
                   std::error_code                ec,
                   boost::leaf::match<std::error_code, boost::leaf::category<neo::sqlite3::errc>>,
                   e_sqlite3_error const* sqlite_error) {
        bpt_log(error,
                "SQLite error while importing [.br.yellow[{}]]"_styled,
                crs_dir.value.string());
        bpt_log(error, "  .br.red[{}]"_styled, ec.message());
        if (sqlite_error) {
            bpt_log(error, "    .br.red[{}]"_styled, sqlite_error->value);
        }
        bpt_log(error, "(It's possible that the database is invalid or corrupted)");
        write_error_marker("repo-import-db-error");
        return 1;
    }
    bpt_leaf_catch(bpt::e_json_parse_error        parse_error,
                   bpt::crs::e_repo_importing_dir crs_dir,
                   bpt::crs::e_pkg_json_path      pkg_json_path) {
        bpt_log(error, "Error while importing [.br.yellow[{}]]"_styled, crs_dir.value.string());
        bpt_log(error,
                "  JSON parse error in [.br.yellow[{}]]:"_styled,
                pkg_json_path.value.string());
        bpt_log(error, "    .br.red[{}]"_styled, parse_error.value);
        write_error_marker("repo-import-invalid-crs-json-parse-error");
        return 1;
    }
    bpt_leaf_catch(bpt::crs::e_invalid_meta_data  error,
                   bpt::crs::e_repo_importing_dir crs_dir,
                   bpt::crs::e_pkg_json_path      pkg_json_path) {
        bpt_log(error, "Error while importing [.br.yellow[{}]]"_styled, crs_dir.value.string());
        bpt_log(error,
                "CRS data in [.br.yellow[{}]] is invalid: .br.red[{}]"_styled,
                pkg_json_path.value.string(),
                error.value);
        write_error_marker("repo-import-invalid-crs-json");
        return 1;
    }
    bpt_leaf_catch(bpt::crs::e_invalid_meta_data  error,
                   bpt::crs::e_repo_importing_dir proj_dir,
                   bpt::e_open_project,
                   bpt::e_parse_project_manifest_path proj_json_path) {
        bpt_log(error, "Error while importing [.br.yellow[{}]]"_styled, proj_dir.value.string());
        bpt_log(error,
                "Project data in [.br.yellow[{}]] is invalid: .br.red[{}]"_styled,
                proj_json_path.value.string(),
                error.value);
        write_error_marker("repo-import-invalid-proj-json");
        return 1;
    }
    bpt_leaf_catch(user_error<errc::invalid_pkg_filesystem> const& exc,
                   crs::e_repo_importing_dir                       crs_dir) {
        bpt_log(error,
                "Error while importing [.br.yellow[{}]]: .br.red[{}]"_styled,
                crs_dir.value.string(),
                exc.what());
        write_error_marker("repo-import-noent");
        return 1;
    }
    bpt_leaf_catch(bpt::crs::e_repo_importing_dir crs_dir,
                   bpt::e_read_file_path const*   read_file,
                   bpt::e_copy_file const*        copy_file,
                   std::error_code                ec) {
        bpt_log(error, "Error while importing [.br.red[{}]]:"_styled, crs_dir.value.string());
        if (read_file) {
            bpt_log(error,
                    " Error reading file [.br.yellow[{}]]:"_styled,
                    read_file->value.string(),
                    ec.message());
        } else if (copy_file) {
            bpt_log(error,
                    " Error copying file [.br.yellow[{}]] to [.br.yellow[{}]]:"_styled,
                    copy_file->source.string(),
                    copy_file->dest.string());
        }
        bpt_log(error, "  .br.red[{}]"_styled, ec.message());
        write_error_marker("repo-import-noent");
        return 1;
    }
    bpt_leaf_catch(crs::e_repo_importing_dir              crs_dir,
                   crs::e_repo_importing_package          meta,
                   crs::e_repo_import_invalid_pkg_version err) {
        bpt_log(info,
                "Error while importing .br.yellow[{}] (from [.br.yellow[{}]]):"_styled,
                meta.value.id.to_string(),
                crs_dir.value.string());
        bpt_log(error, "Invalid 'pkg-version' on package: .br.red[{}]"_styled, err.value);
        write_error_marker("repo-import-invalid-pkg-version");
        return 1;
    };
}

}  // namespace bpt::cli::cmd
