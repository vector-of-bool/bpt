#pragma once

#include <dds/error/result_fwd.hpp>

#include <neo/sqlite3/fwd.hpp>
#include <neo/sqlite3/literal.hpp>

#include <memory>

namespace dds {

struct e_db_open_path {
    std::string value;
};

struct e_db_open_ec {
    std::error_code value;
};

struct e_sqlite3_error {
    std::string value;
};

class unique_database {
    struct impl;

    std::unique_ptr<impl> _impl;

    explicit unique_database(std::unique_ptr<impl> db) noexcept;

public:
    [[nodiscard]] static result<unique_database> open(const std::string& str) noexcept;
    [[nodiscard]] static result<unique_database> open_existing(const std::string& str) noexcept;

    ~unique_database();
    unique_database(unique_database&&) noexcept;
    unique_database& operator=(unique_database&&) noexcept;

    neo::sqlite3::connection_ref raw_database() const noexcept;
    neo::sqlite3::statement&     prepare(neo::sqlite3::sql_string_literal) const;

    void exec_script(neo::sqlite3::sql_string_literal);
};

}  // namespace dds
