#ifndef DDS_UTIL_HPP_INCLUDED
#define DDS_UTIL_HPP_INCLUDED

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace dds {

namespace detail {

template <std::size_t I,
          typename A = std::nullptr_t,
          typename B = std::nullptr_t,
          typename C = std::nullptr_t,
          typename D = std::nullptr_t>
decltype(auto) nth_arg(A&& a = nullptr, B&& b = nullptr, C&& c = nullptr, D&& d = nullptr) {
    if constexpr (I == 0) {
        return (A &&) a;
    } else if constexpr (I == 1) {
        return (B &&) b;
    } else if constexpr (I == 2) {
        return (C &&) c;
    } else if constexpr (I == 3) {
        return (D &&) d;
    }
}

}  // namespace detail

// Based on https://github.com/Quincunx271/TerseLambda
#define DDS_CTL(...)                                                                               \
    (auto&&... _args_)->auto {                                                                     \
        [[maybe_unused]] auto&& _1 = ::dds::detail::nth_arg<0>((decltype(_args_)&&)(_args_)...);   \
        [[maybe_unused]] auto&& _2 = ::dds::detail::nth_arg<1>((decltype(_args_)&&)(_args_)...);   \
        [[maybe_unused]] auto&& _3 = ::dds::detail::nth_arg<2>((decltype(_args_)&&)(_args_)...);   \
        [[maybe_unused]] auto&& _4 = ::dds::detail::nth_arg<3>((decltype(_args_)&&)(_args_)...);   \
        static_assert(sizeof...(_args_) <= 4);                                                     \
        return (__VA_ARGS__);                                                                      \
    }

#define DDS_TL [&] DDS_CTL

inline namespace file_utils {

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

void safe_rename(path_ref source, path_ref dest);

}  // namespace file_utils

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

inline namespace string_utils {

inline std::string_view sview(std::string_view::const_iterator beg,
                              std::string_view::const_iterator end) {
    return std::string_view(&*beg, static_cast<std::size_t>(std::distance(beg, end)));
}

inline std::string_view trim(std::string_view s) {
    auto iter = s.begin();
    auto end  = s.end();
    while (iter != end && std::isspace(*iter)) {
        ++iter;
    }
    auto riter = s.rbegin();
    auto rend  = s.rend();
    while (riter != rend && std::isspace(*riter)) {
        ++riter;
    }
    auto new_end = riter.base();
    return sview(iter, new_end);
}

inline std::string trim(std::string&& s) { return std::string(trim(s)); }

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

}  // namespace string_utils

}  // namespace dds

#endif  // DDS_UTIL_HPP_INCLUDED