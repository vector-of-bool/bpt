#include "./handle.hpp"

#include <dds/util/log.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <boost/leaf/result.hpp>
#include <fmt/ostream.h>

using namespace dds;

void dds::leaf_handle_unknown_void(std::string_view                            message,
                                   const boost::leaf::verbose_diagnostic_info& info) {
    dds_log(warn, message);
    dds_log(warn, "An unhandled error occurred:\n{}", info);
}
