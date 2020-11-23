#include "./remote.hpp"

#include <dds/error/errors.hpp>
#include <dds/http/session.hpp>
#include <dds/temp.hpp>
#include <dds/util/result.hpp>

#include <neo/sqlite3/exec.hpp>
#include <neo/sqlite3/single.hpp>
#include <neo/sqlite3/transaction.hpp>
#include <neo/url.hpp>
#include <neo/utility.hpp>

using namespace dds;
namespace nsql = neo::sqlite3;

namespace {

struct remote_db {
    temporary_dir  _tempdir;
    nsql::database db;

    static remote_db download_and_open(neo::url const& url) {
        neo_assert(expects,
                   url.host.has_value(),
                   "URL does not have a hostname??",
                   url.to_string());
        auto sess = url.scheme == "https"
            ? http_session::connect_ssl(*url.host, url.port_or_default_port_or(443))
            : http_session::connect(*url.host, url.port_or_default_port_or(80));

        auto tempdir    = temporary_dir::create();
        auto repo_db_dl = tempdir.path() / "repo.db";
        fs::create_directories(tempdir.path());
        sess.download_file(
            {
                .method = "GET",
                .path   = url.path,
            },
            repo_db_dl);

        auto db = nsql::open(repo_db_dl.string());
        return {tempdir, std::move(db)};
    }

    static remote_db download_and_open_for_base(neo::url url) {
        auto repo_url = url;
        repo_url.path = fs::path(url.path).append("repo.db").string();
        return download_and_open(repo_url);
    }

    static remote_db download_and_open_for_base(std::string_view url_str) {
        return download_and_open_for_base(neo::url::parse(url_str));
    }
};

}  // namespace

remote_repository remote_repository::connect(std::string_view url_str) {
    DDS_E_SCOPE(e_url_string{std::string(url_str)});
    const auto url = neo::url::parse(url_str);

    auto db      = remote_db::download_and_open_for_base(url);
    auto name_st = db.db.prepare("SELECT name FROM dds_repo_meta");
    auto [name]  = nsql::unpack_single<std::string>(name_st);

    remote_repository ret;
    ret._base_url = url;
    ret._name     = name;
    return ret;
}

void remote_repository::store(nsql::database_ref db) {
    auto st = db.prepare(R"(
        INSERT INTO dds_cat_remotes (name, gen_ident, remote_url)
            VALUES (?, ?, ?)
    )");
    nsql::exec(st, _name, "[placeholder]", _base_url.to_string());
}

void remote_repository::update_catalog(nsql::database_ref db) {
    auto rdb = remote_db::download_and_open_for_base(_base_url);

    auto db_path = rdb._tempdir.path() / "repo.db";

    auto rid_st          = db.prepare("SELECT remote_id FROM dds_cat_remotes WHERE name = ?");
    rid_st.bindings()[1] = _name;
    auto [remote_id]     = nsql::unpack_single<std::int64_t>(rid_st);

    nsql::transaction_guard tr{db};
    nsql::exec(db.prepare("ATTACH DATABASE ? AS remote"), db_path.string());
    nsql::exec(  //
        db.prepare(R"(
            DELETE FROM dds_cat_pkgs
            WHERE remote_id = ?
        )"),
        remote_id);
    nsql::exec(  //
        db.prepare(R"(
            INSERT INTO dds_cat_pkgs
                (name, version, description, remote_url, remote_id)
            SELECT
                name,
                version,
                description,
                printf('dds:%s/%s', name, version),
                ?1
            FROM remote.dds_repo_packages
        )"),
        remote_id);
}
