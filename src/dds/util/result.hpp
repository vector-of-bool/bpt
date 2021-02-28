#pragma once

#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>

#include <boost/leaf/handle_error.hpp>
#include <boost/leaf/on_error.hpp>
#include <boost/leaf/result.hpp>
#include <neo/concepts.hpp>
#include <neo/fwd.hpp>
#include <neo/pp.hpp>

#include <exception>
#include <filesystem>
#include <string>
#include <string_view>

namespace dds {

using boost::leaf::current_error;
using boost::leaf::error_id;
using boost::leaf::new_error;
using boost::leaf::result;

template <typename T, neo::convertible_to<T> U>
constexpr T value_or(const result<T>& res, U&& arg) {
    return res ? res.value() : static_cast<T>(arg);
}

template <auto Val>
using matchv = boost::leaf::match<decltype(Val), Val>;

/**
 * @brief Error object representing a captured system_error exception
 */
struct e_system_error_exc {
    std::string     message;
    std::error_code code;
};

struct e_url_string {
    std::string value;
};

struct e_missing_file {
    std::filesystem::path path;
};

/**
 * @brief Capture currently in-flight special exceptions as new error object. Works around a bug in
 * Boost.LEAF when catching std::system error.
 */
[[noreturn]] void capture_exception();

void write_error_marker(std::string_view error) noexcept;

}  // namespace dds
