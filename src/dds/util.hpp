#ifndef DDS_UTIL_HPP_INCLUDED
#define DDS_UTIL_HPP_INCLUDED

#include <filesystem>
#include <fstream>
#include <algorithm>

namespace dds {

namespace fs = std::filesystem;

std::fstream open(const fs::path& filepath, std::ios::openmode mode, std::error_code& ec);
std::string  slurp_file(const fs::path& path, std::error_code& ec);

inline std::fstream open(const fs::path& filepath, std::ios::openmode mode) {
    std::error_code ec;
    auto            ret = dds::open(filepath, mode, ec);
    if (ec) {
        throw std::system_error{ec, "Error opening file: " + filepath.string()};
    }
    return ret;
}

inline std::string slurp_file(const fs::path& path) {
    std::error_code ec;
    auto            contents = dds::slurp_file(path, ec);
    if (ec) {
        throw std::system_error{ec, "Reading file: " + path.string()};
    }
    return contents;
}

inline bool ends_with(std::string_view s, std::string_view key) {
    auto found = s.find(key);
    return found != s.npos && found == s.size() - key.size();
}

template <typename Container, typename Predicate>
void erase_if(Container& c, Predicate&& p) {
    auto erase_point = std::remove_if(c.begin(), c.end(), p);
    c.erase(erase_point, c.end());
}

template <typename Container, typename Other>
void extend(Container& c, const Other& o) {
    c.insert(c.end(), o.begin(), o.end());
}

}  // namespace dds

#endif  // DDS_UTIL_HPP_INCLUDED