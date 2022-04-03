#include "./handle.hpp"

#include <bpt/util/log.hpp>

#include <boost/leaf.hpp>
#include <fmt/ostream.h>

using namespace bpt;

void bpt::leaf_handle_unknown_void(std::string_view                            message,
                                   const boost::leaf::verbose_diagnostic_info& info) {
    bpt_log(warn, message);
    bpt_log(warn, "An unhandled error occurred:\n{}", info);
}
