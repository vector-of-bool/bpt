#include "./db.hpp"

#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>

#include <boost/leaf/exception.hpp>
#include <neo/sqlite3/database.hpp>
#include <neo/sqlite3/error.hpp>
#include <neo/sqlite3/statement.hpp>
#include <neo/sqlite3/statement_cache.hpp>

#include <filesystem>

using namespace dds;
namespace nsql = neo::sqlite3;

struct unique_database::impl {
    nsql::connection      db;
    nsql::statement_cache cache{db};
};

unique_database::unique_database(std::unique_ptr<unique_database::impl> iptr) noexcept
    : _impl(std::move(iptr)) {}

unique_database::~unique_database() noexcept                 = default;
unique_database::unique_database(unique_database&&) noexcept = default;
unique_database& unique_database::operator=(unique_database&&) noexcept = default;

static result<nsql::connection> _open_db(neo::zstring_view str, nsql::openmode mode) noexcept {
    DDS_E_SCOPE(e_db_open_path{std::string(str)});
    auto db = nsql::connection::open(str, mode);
    if (!db.has_value()) {
        return new_error(e_db_open_ec{nsql::make_error_code(db.errc())});
    }
    db->exec("PRAGMA foreign_keys = 1;").throw_if_error();
    db->exec("PRAGMA vdbe_trace = 1;").throw_if_error();
    return std::move(*db);
}

result<unique_database> unique_database::open(neo::zstring_view str) noexcept {
    BOOST_LEAF_AUTO(db, _open_db(str, nsql::openmode::readwrite | nsql::openmode::create));
    return unique_database{std::make_unique<impl>(std::move(db))};
}

result<unique_database> unique_database::open_existing(neo::zstring_view str) noexcept {
    BOOST_LEAF_AUTO(db, _open_db(str, nsql::openmode::readwrite));
    return unique_database{std::make_unique<impl>(std::move(db))};
}

void unique_database::exec_script(nsql::sql_string_literal script) {
    _impl->db.exec(neo::zstring_view(script.string())).throw_if_error();
}

nsql::connection_ref unique_database::raw_database() const noexcept { return _impl->db; }

nsql::statement& unique_database::prepare(nsql::sql_string_literal s) const {
    try {
        return _impl->cache(s);
    } catch (const neo::sqlite3::error& exc) {
        auto err = new_error(e_sqlite3_error{exc.db_message()}, exc.code());
        if (exc.code().category() == neo::sqlite3::error_category()) {
            err.load(neo::sqlite3::errc{exc.code().value()});
        }
        throw;
    }
}
