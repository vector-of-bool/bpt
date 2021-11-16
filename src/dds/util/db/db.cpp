#include "./db.hpp"

#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>

#include <neo/sqlite3/database.hpp>
#include <neo/sqlite3/statement.hpp>
#include <neo/sqlite3/statement_cache.hpp>

using namespace dds;
namespace nsql = neo::sqlite3;

struct unique_database::impl {
    nsql::database        db;
    nsql::statement_cache cache{db};
};

unique_database::unique_database(std::unique_ptr<unique_database::impl> iptr) noexcept
    : _impl(std::move(iptr)) {}

unique_database::~unique_database() noexcept                 = default;
unique_database::unique_database(unique_database&&) noexcept = default;
unique_database& unique_database::operator=(unique_database&&) noexcept = default;

result<unique_database> unique_database::open(const std::string& str) noexcept {
    std::error_code ec;
    DDS_E_SCOPE(ec);
    auto db = nsql::database::open(str, ec);
    if (ec) {
        return new_error();
    }
    db->exec("PRAGMA foreign_keys = 1;");
    auto p = std::make_unique<impl>(std::move(*db));
    return unique_database{std::move(p)};
}

void unique_database::exec_script(nsql::sql_string_literal script) {
    _impl->db.exec(std::string(script.string()));
}

nsql::database_ref unique_database::raw_database() const noexcept { return _impl->db; }

nsql::statement& unique_database::prepare(nsql::sql_string_literal s) noexcept {
    return _impl->cache(s);
}
