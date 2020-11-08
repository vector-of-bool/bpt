#include "./fs.hpp"

#include <fmt/core.h>

#include <sstream>

using namespace dds;

std::fstream dds::open(const fs::path& filepath, std::ios::openmode mode, std::error_code& ec) {
    std::fstream ret;
    auto         mask = ret.exceptions() | std::ios::badbit;
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

void dds::safe_rename(path_ref source, path_ref dest) {
    std::error_code ec;
    fs::rename(source, dest, ec);
    if (!ec) {
        return;
    }

    if (ec != std::errc::cross_device_link && ec != std::errc::permission_denied) {
        throw std::system_error(ec,
                                fmt::format("Failed to move item [{}] to [{}]",
                                            source.string(),
                                            dest.string()));
    }

    auto tmp = dest;
    tmp.replace_filename(tmp.filename().string() + ".tmp-dds-mv");
    try {
        fs::remove_all(tmp);
        fs::copy(source, tmp, fs::copy_options::recursive);
    } catch (...) {
        fs::remove_all(tmp, ec);
        throw;
    }
    fs::rename(tmp, dest);
    fs::remove_all(source);
}