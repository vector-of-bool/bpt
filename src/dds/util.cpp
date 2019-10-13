#include "./util.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

std::fstream dds::open(const fs::path& filepath, std::ios::openmode mode, std::error_code& ec) {
    std::fstream ret;
    auto         mask = ret.exceptions() | std::ios::failbit;
    ret.exceptions(mask);

    try {
        ret.open(filepath.string(), mode);
    } catch (const std::ios::failure&) {
        ec = std::error_code(errno, std::system_category());
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
