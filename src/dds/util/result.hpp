#pragma once

#include <boost/leaf/on_error.hpp>
#include <boost/leaf/result.hpp>
#include <neo/concepts.hpp>
#include <neo/fwd.hpp>
#include <neo/pp.hpp>

#include <exception>
#include <filesystem>
#include <string>

namespace dds {

using boost::leaf::current_error;
using boost::leaf::error_id;
using boost::leaf::new_error;

template <typename T>
class result : public boost::leaf::result<T> {
public:
    using result_base = boost::leaf::result<T>;

    using result_base::result;

    template <neo::convertible_to<result_base> Other>
    result(Other&& oth) noexcept
        : result_base(static_cast<result_base>(NEO_FWD(oth))) {}

    constexpr bool has_value() const noexcept { return !!*this; }
    template <typename U>
    constexpr T value_or(U&& u) const noexcept {
        if (has_value()) {
            return **this;
        } else {
            return T(NEO_FWD(u));
        }
    }
};

/**
 * @brief Error object representing a captured system_error exception
 */
struct e_system_error_exc {
    std::string     message;
    std::error_code code;
};

/**
 * @brief Error object representing a captured neo::sqlite3::sqlite3_error
 */
struct e_sqlite3_error_exc {
    std::string     message;
    std::error_code code;
};

struct e_url_string {
    std::string value;
};

struct e_human_message {
    std::string value;
};

struct e_missing_file {
    std::filesystem::path path;
};

struct e_error_marker {
    std::string_view value;
};

struct e_parse_error {
    std::string value;
};

/**
 * @brief Capture currently in-flight special exceptions as new error object. Works around a bug in
 * Boost.LEAF when catching std::system error.
 */
[[noreturn]] void capture_exception();

#define DDS_E_ARG(...) ([&] { return __VA_ARGS__; })

#define DDS_ERROR_MARKER(Value) DDS_E_ARG(::dds::e_error_marker{Value})

/**
 * @brief Generate a leaf::on_error object that loads the given expression into the currently
 * in-flight error if the current scope is exitted via exception or a bad result<>
 */
#define DDS_E_SCOPE(...)                                                                           \
    auto NEO_CONCAT(_err_info_, __LINE__) = boost::leaf::on_error(DDS_E_ARG(__VA_ARGS__))

}  // namespace dds

template <typename T>
struct boost::leaf::is_result_type<dds::result<T>> : std::true_type {};
