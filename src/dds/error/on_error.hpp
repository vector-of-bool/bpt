#pragma once

#include <neo/pp.hpp>

#include <boost/leaf/on_error.hpp>

/**
 * @brief Generate a callable object that returns the given expression.
 *
 * Use this as a parameter to leaf's error-loading APIs.
 */
#define DDS_E_ARG(...) ([&] { return __VA_ARGS__; })

/**
 * @brief Generate a leaf::on_error object that loads the given expression into the currently
 * in-flight error if the current scope is exitted via exception or a bad result<>
 */
#define DDS_E_SCOPE(...)                                                                           \
    auto NEO_CONCAT(_err_info_, __LINE__) = boost::leaf::on_error(DDS_E_ARG(__VA_ARGS__))
