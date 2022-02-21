#include "./io.hpp"

#include <dds/error/on_error.hpp>

#include <boost/leaf/common.hpp>
#include <boost/leaf/exception.hpp>
#include <neo/ufmt.hpp>

#include <fstream>

using namespace dds;

using path_ref = const std::filesystem::path&;

std::fstream dds::open_file(path_ref fpath, std::ios::openmode mode) {
    DDS_E_SCOPE(e_open_file_path{fpath});
    errno = 0;
    std::fstream ret{fpath, mode};
    auto         e = errno;
    if (!ret) {
        auto ec = std::error_code{e, std::system_category()};
        BOOST_LEAF_THROW_EXCEPTION(std::system_error(ec,
                                                     neo::ufmt("Failed to open file [{}]",
                                                               fpath.string())),
                                   boost::leaf::e_errno{e},
                                   ec);
    }
    return ret;
}

void dds::write_file(path_ref dest, std::string_view content) {
    DDS_E_SCOPE(e_write_file_path{dest});
    auto ofile = open_file(dest, std::ios::binary | std::ios::out);
    errno      = 0;
    ofile.write(content.data(), content.size());
    auto e = errno;
    if (!ofile) {
        auto ec = std::error_code(e, std::system_category());
        BOOST_LEAF_THROW_EXCEPTION(std::system_error(ec,
                                                     neo::ufmt("Failed to write to file [{}]",
                                                               dest.string())),
                                   boost::leaf::e_errno{e},
                                   ec);
    }
}

std::string dds::read_file(path_ref path) {
    DDS_E_SCOPE(e_read_file_path{path});
    auto               infile = open_file(path, std::ios::binary | std::ios::in);
    std::ostringstream out;
    out << infile.rdbuf();
    return std::move(out).str();
}
