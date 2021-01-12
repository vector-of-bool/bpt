#include "./fs.hpp"

#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>

#include <fmt/core.h>

#include <sstream>

using namespace dds;

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

result<void> dds::copy_file(path_ref source, path_ref dest, fs::copy_options opts) noexcept {
    std::error_code ec;
    fs::copy_file(source, dest, opts, ec);
    if (ec) {
        return new_error(DDS_E_ARG(e_copy_file{source, dest}), ec);
    }
    return {};
}

result<void> dds::remove_file(path_ref file) noexcept {
    std::error_code ec;
    fs::remove(file, ec);
    if (ec) {
        return new_error(DDS_E_ARG(e_remove_file{file}), ec);
    }
    return {};
}

result<void> dds::create_symlink(path_ref target, path_ref symlink) noexcept {
    std::error_code ec;
    if (fs::is_directory(target)) {
        fs::create_directory_symlink(target, symlink, ec);
    } else {
        fs::create_symlink(target, symlink, ec);
    }
    if (ec) {
        return new_error(DDS_E_ARG(e_symlink{symlink, target}), ec);
    }
    return {};
}

result<void> dds::write_file(path_ref dest, std::string_view content) noexcept {
    std::error_code ec;
    auto            outfile = dds::open(dest, std::ios::binary | std::ios::out, ec);
    if (ec) {
        return new_error(DDS_E_ARG(e_write_file_path{dest}), ec);
    }
    errno = 0;
    outfile.write(content.data(), content.size());
    auto e = errno;
    if (!outfile) {
        return new_error(std::error_code(e, std::system_category()),
                         DDS_E_ARG(e_write_file_path{dest}));
    }
    return {};
}