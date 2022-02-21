#pragma once

#include <dds/error/handle.hpp>
#include <dds/error/marker.hpp>
#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>

#include <boost/leaf.hpp>
#include <boost/leaf/on_error.hpp>
#include <boost/leaf/result.hpp>
#include <neo/concepts.hpp>

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

void write_error_marker(std::string_view error) noexcept;

}  // namespace dds
