#include "./result.hpp"

#include <neo/sqlite3/error.hpp>

void dds::capture_exception() {
    try {
        throw;
    } catch (const neo::sqlite3::sqlite3_error& e) {
        current_error().load(e_sqlite3_error_exc{std::string(e.what()), e.code()},
                             e.code(),
                             neo::sqlite3::errc{e.code().value()});
    } catch (const std::system_error& e) {
        current_error().load(e_system_error_exc{std::string(e.what()), e.code()}, e.code());
    }
    // Re-throw as a bare exception.
    throw std::exception();
}
