#pragma once

#include <initializer_list>
#include <string>
#include <string_view>

#include <dds/error/result.hpp>

namespace dds {

class unique_database;

/// Error value returned when a migration fails
struct e_migration_error {
    std::string value;
};

namespace detail {

/// type-erasure for migration functions
struct erased_migration {
    virtual void apply(unique_database&) = 0;
};

/// Impl for type-erased migration functions
template <typename Func>
struct migration_fn : erased_migration {
    Func& fn;

    explicit migration_fn(Func& f)
        : fn(f) {}

    void apply(unique_database& db) override { fn(db); }
};

/// Do type-erased migrations
result<void> do_migrations_1(unique_database&                         db,
                             std::string_view                         tablename,
                             std::initializer_list<erased_migration*> migrations);

/// type-erasing ship
template <typename... Func>
result<void> do_migrations(unique_database& db, std::string_view tablename, Func&&... migrations) {
    /// Convert the given migrations to pointers-to-base:
    std::initializer_list<erased_migration*> migrations_il = {&migrations...};
    /// Run those  migrations
    return do_migrations_1(db, tablename, migrations_il);
}

}  // namespace detail

/// Get the migration meta version from the database stored in the given table name
result<int> get_migration_version(unique_database& db, std::string_view tablename);

/**
 * @brief Execute database migrations on the database, and update the meta table "tablename"
 */
template <typename... Func>
[[nodiscard]] result<void>
apply_db_migrations(unique_database& db, std::string_view tablename, Func&&... fn) {
    return detail::do_migrations(db, tablename, detail::migration_fn<Func>{fn}...);
}

}  // namespace dds
