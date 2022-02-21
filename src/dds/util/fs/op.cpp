#include "./op.hpp"

#include <dds/error/on_error.hpp>

#include <boost/leaf/exception.hpp>
#include <neo/ufmt.hpp>

using namespace dds;

bool file_utils::file_exists(const std::filesystem::path& filepath) {
    std::error_code ec;
    auto            r = std::filesystem::exists(filepath, ec);
    if (ec) {
        BOOST_LEAF_THROW_EXCEPTION(
            std::system_error{ec,
                              neo::ufmt("Error checking for the existence of a file [{}]",
                                        filepath.string())},
            filepath,
            ec);
    }
    return r;
}
