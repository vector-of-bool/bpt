#ifndef DDS_UTIL_HPP_INCLUDED
#define DDS_UTIL_HPP_INCLUDED

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace dds {

namespace fs = std::filesystem;

using path_ref = const fs::path&;

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
    auto found = s.rfind(key);
    return found != s.npos && found == s.size() - key.size();
}

inline bool starts_with(std::string_view s, std::string_view key) { return s.find(key) == 0; }

inline bool contains(std::string_view s, std::string_view key) { return s.find(key) != s.npos; }

inline std::vector<std::string> split(std::string_view str, std::string_view sep) {
    std::vector<std::string>    ret;
    std::string_view::size_type prev_pos = 0;
    auto                        pos      = prev_pos;
    while ((pos = str.find(sep, prev_pos)) != str.npos) {
        ret.emplace_back(str.substr(prev_pos, pos - prev_pos));
        prev_pos = pos + sep.length();
    }
    ret.emplace_back(str.substr(prev_pos));
    return ret;
}

inline std::string replace(std::string_view str, std::string_view key, std::string_view repl) {
    std::string                 ret;
    std::string_view::size_type pos      = 0;
    std::string_view::size_type prev_pos = 0;
    while (pos = str.find(key, pos), pos != key.npos) {
        ret.append(str.begin() + prev_pos, str.begin() + pos);
        ret.append(repl);
        prev_pos = pos += key.size();
    }
    ret.append(str.begin() + prev_pos, str.end());
    return ret;
}

inline std::vector<std::string>
replace(std::vector<std::string> strings, std::string_view key, std::string_view repl) {
    for (auto& item : strings) {
        item = replace(item, key, repl);
    }
    return strings;
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

template <typename Container, typename Item>
void extend(Container& c, std::initializer_list<Item> il) {
    c.insert(c.end(), il.begin(), il.end());
}

}  // namespace dds

#endif  // DDS_UTIL_HPP_INCLUDED