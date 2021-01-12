#include "./result.hpp"

#include <dds/util/env.hpp>
#include <dds/util/log.hpp>

#include <fmt/ostream.h>
#include <neo/sqlite3/error.hpp>

#include <fstream>

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
    // Re-throw as a bad_result exception with the same error ID
    throw boost::leaf::bad_result(boost::leaf::current_error());
}

void dds::write_error_marker(std::string_view error) noexcept {
    dds_log(trace, "[error marker {}]", error);
    auto efile_path = dds::getenv("DDS_WRITE_ERROR_MARKER");
    if (efile_path) {
        std::ofstream outfile{*efile_path, std::ios::binary};
        fmt::print(outfile, "{}", error);
    }
}
