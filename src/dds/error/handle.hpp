#pragma once

#include <neo/fwd.hpp>

#include <boost/leaf/pred.hpp>

#include <string_view>

namespace boost::leaf {

class verbose_diagnostic_info;

}  // namespace boost::leaf

namespace dds {

void leaf_handle_unknown_void(std::string_view message,
                              const boost::leaf::verbose_diagnostic_info&);

template <typename T>
auto leaf_handle_unknown(std::string_view message, T&& val) {
    return [val = NEO_FWD(val), message](const boost::leaf::verbose_diagnostic_info& info) {
        leaf_handle_unknown_void(message, info);
        return val;
    };
}

template <auto Val>
using matchv = boost::leaf::match<decltype(Val), Val>;

}  // namespace dds
