#pragma once

#include <bpt/error/result.hpp>

#include <neo/sqlite3/error.hpp>
#include <neo/sqlite3/iter_tuples.hpp>
#include <neo/sqlite3/statement.hpp>

#include "./db.hpp"

namespace dds {

template <typename... Args>
[[nodiscard]] result<void> db_bind(neo::sqlite3::statement& st, const Args&... args) noexcept {
    auto rc = st.bindings().bind_all(args...);
    if (rc.is_error()) {
        return new_error(rc.errc(), make_error_code(rc.errc()));
    }
    return {};
}

/**
 * @brief Execute a database query and return a lazy tuple iterator for the given Ts...
 */
template <typename... Ts, typename... Args>
[[nodiscard]] auto db_query(neo::sqlite3::statement& st, const Args&... args) {
    neo_assert(expects,
               !st.is_busy(),
               "db_query<>() started on a statement that is already executing. Someone forgot to "
               "call .reset()");
    st.reset();
    st.bindings().bind_all(args...).throw_if_error();
    return neo::sqlite3::iter_tuples<Ts...>{st};
}

/**
 * @brief Obtain a single row from the database using the given query.
 */
template <typename... Ts, typename... Args>
[[nodiscard]] result<std::tuple<Ts...>> db_single(neo::sqlite3::statement_mutref st,
                                                  const Args&... args) {
    st->reset();
    BOOST_LEAF_CHECK(db_bind(st, args...));
    auto rc = st->step();
    if (rc != neo::sqlite3::errc::row) {
        return new_error(rc.errc(), make_error_code(rc.errc()));
    }
    auto tup = st->row().template unpack<Ts...>().as_tuple();
    st->reset();
    return tup;
}

/**
 * @brief Obtain a single value from the database of the given type T
 */
template <typename T, typename... Args>
[[nodiscard]] result<T> db_cell(neo::sqlite3::statement_mutref st, const Args&... args) {
    BOOST_LEAF_AUTO(row, db_single<T>(st, args...));
    auto [value] = NEO_FWD(row);
    return value;
}

/**
 * @brief Execute the given SQL statement until completion.
 */
template <typename... Args>
[[nodiscard]] result<void> db_exec(neo::sqlite3::statement_mutref st, const Args&... args) {
    st->reset();
    BOOST_LEAF_CHECK(db_bind(st, args...));
    auto rst = st->auto_reset();
    auto rc  = st->run_to_completion();
    if (rc.is_error()) {
        return new_error(rc.errc(), make_error_code(rc.errc()));
    }
    return {};
}

}  // namespace dds
