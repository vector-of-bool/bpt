#include "./fs.hpp"

#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>

#include <boost/leaf/common.hpp>
#include <boost/leaf/exception.hpp>
#include <fmt/core.h>

#include <sstream>

using namespace dds;

result<std::fstream> dds::open_file(path_ref fpath, std::ios::openmode mode) noexcept {
    errno = 0;
    std::fstream ret{fpath, mode};
    auto         e = errno;
    if (!ret) {
        return new_error(boost::leaf::e_errno{e},
                         e_open_file_path{fpath},
                         std::error_code{e, std::system_category()});
    }
    return ret;
}

result<void> dds::write_file(path_ref dest, std::string_view content) noexcept {
    DDS_E_SCOPE(e_write_file_path{dest});
    BOOST_LEAF_AUTO(outfile, open_file(dest, std::ios::binary | std::ios::out));
    errno = 0;
    outfile.write(content.data(), content.size());
    auto e = errno;
    if (!outfile) {
        return new_error(boost::leaf::e_errno{e}, std::error_code(e, std::system_category()));
    }
    return {};
}

result<std::string> dds::read_file(path_ref path) noexcept {
    DDS_E_SCOPE(e_read_file_path{path});
    BOOST_LEAF_AUTO(infile, open_file(path, std::ios::binary | std::ios::in));
    std::ostringstream out;
    out << infile.rdbuf();
    return std::move(out).str();
}

std::fstream dds::open(const fs::path& filepath, std::ios::openmode mode, std::error_code& ec) {
    std::fstream ret{filepath, mode};
    if (!ret) {
        ec = std::error_code(errno, std::system_category());
    }
    return ret;
}

std::fstream dds::open(const fs::path& filepath, std::ios::openmode mode) {
    DDS_E_SCOPE(e_open_file_path{filepath.string()});
    std::error_code ec;
    auto            ret = dds::open(filepath, mode, ec);
    if (ec) {
        BOOST_LEAF_THROW_EXCEPTION(std::system_error{ec,
                                                     "Error opening file: " + filepath.string()},
                                   ec);
    }
    return ret;
}

std::string dds::slurp_file(const fs::path& path, std::error_code& ec) {
    auto file = dds::open(path, std::ios::in, ec);
    if (ec) {
        return std::string{};
    }

    std::ostringstream out;
    out << file.rdbuf();
    return std::move(out).str();
}

std::string dds::slurp_file(const fs::path& path) {
    DDS_E_SCOPE(e_read_file_path{path.string()});
    std::error_code ec;
    auto            contents = dds::slurp_file(path, ec);
    if (ec) {
        BOOST_LEAF_THROW_EXCEPTION(std::system_error{ec, "Reading file: " + path.string()}, ec);
    }
    return contents;
}
