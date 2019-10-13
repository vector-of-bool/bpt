#ifndef DDS_LOGGING_HPP_INCLUDED
#define DDS_LOGGING_HPP_INCLUDED

#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace dds {

inline auto get_logger() { return spdlog::stdout_color_mt("console"); }

} // namespace dds

#endif // DDS_LOGGING_HPP_INCLUDED