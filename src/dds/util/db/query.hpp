#pragma once

#include <neo/sqlite3/iter_tuples.hpp>
#include <neo/sqlite3/statement.hpp>

#include "./db.hpp"

namespace dds {

/**
 * @brief Execute a database query and return a lazy tuple iterator for the given Ts...
 */
template <typename... Ts, typename... Args>
auto db_query(neo::sqlite3::statement& st, const Args&... args) {
    neo_assert(expects,
               !st.is_busy(),
               "db_query<>() started on a statement that is already executing. Someone forgot to "
               "call .reset()");
    st.bindings().bind_all(args...).throw_if_error();
    return neo::sqlite3::iter_tuples<Ts...>{st};
}

/**
 * @brief Obtain a single row from the database using the given query.
 */
template <typename... Ts, typename... Args>
auto db_single(neo::sqlite3::statement_mutref st, const Args&... args) {
    auto tups = db_query<Ts...>(st, args...);
    auto rst  = st->auto_reset();
    return tups.begin()->as_tuple();
}

/**
 * @brief Obtain a single value from the database of the given type T
 */
template <typename T, typename... Args>
auto db_cell(neo::sqlite3::statement_mutref st, const Args&... args) {
    auto tup     = db_single<T>(st, args...);
    auto [value] = NEO_FWD(tup);
    return value;
}

/**
 * @brief Execute the given SQL statement until completion.
 */
template <typename... Args>
auto db_exec(neo::sqlite3::statement_mutref st, const Args&... args) {
    auto rst = st->auto_reset();
    st->bindings().bind_all(args...).throw_if_error();
    st->run_to_completion().throw_if_error();
}

}  // namespace dds
