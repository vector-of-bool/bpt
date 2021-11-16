#pragma once

#include <neo/sqlite3/iter_tuples.hpp>
#include <neo/sqlite3/statement.hpp>

#include "./db.hpp"

namespace dds {

namespace detail {

/// Prepare and bind a db statement with the given binding arguments.
template <typename... Args>
neo::sqlite3::statement& db_prepare_and_bind(unique_database&                 db,
                                             neo::sqlite3::sql_string_literal s,
                                             std::tuple<Args...> const&       args) {
    neo::sqlite3::statement& st = db.prepare(s);
    st.reset();
    st.bindings() = args;
    return st;
}

}  // namespace detail

/**
 * @brief Execute a database query and return a lazy tuple iterator for the given Ts...
 */
template <typename... Ts, typename... Args>
auto db_query(unique_database&                 db,
              neo::sqlite3::sql_string_literal s,
              std::tuple<Args...> const&       args) {
    auto& st = detail::db_prepare_and_bind(db, s, args);
    return neo::sqlite3::iter_tuples<Ts...>{st};
}

template <typename... Ts>
auto db_query(unique_database& db, neo::sqlite3::sql_string_literal s) {
    return db_query<Ts...>(db, s, std::tie());
}

/**
 * @brief Obtain a single value from the database of the given type T
 */
template <typename T, typename... Args>
auto db_cell(unique_database&                 db,
             neo::sqlite3::sql_string_literal s,
             std::tuple<Args...> const&       args) {
    auto iter = db_query<T>(db, s, args).begin();
    neo_assert(invariant,
               !iter.at_end(),
               "db_cell() should always have exactly one value",
               s.string());
    auto value = std::get<0>(std::move(*iter));
    ++iter;
    neo_assert(invariant, iter.at_end(), "db_cell() should only return exactly one row");
    return value;
}

template <typename T>
auto db_cell(unique_database& db, neo::sqlite3::sql_string_literal s) {
    return db_cell<T>(db, s, std::tie());
}

/**
 * @brief Execute the given SQL statement until completion.
 */
template <typename... Args>
auto db_exec(unique_database&                 db,
             neo::sqlite3::sql_string_literal s,
             std::tuple<Args...> const&       args) {
    auto& st = detail::db_prepare_and_bind(db, s, args);
    st.run_to_completion();
}

template <typename... Ts>
auto db_exec(unique_database& db, neo::sqlite3::sql_string_literal s) {
    db_exec<Ts...>(db, s, std::tie());
}

}  // namespace dds