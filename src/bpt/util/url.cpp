#include "./url.hpp"

#include <bpt/error/on_error.hpp>

#include <neo/utility.hpp>

#include <filesystem>

using namespace bpt;

neo::url bpt::parse_url(std::string_view sv) {
    BPT_E_SCOPE(e_url_string{std::string(sv)});
    return neo::url::parse(sv);
}

neo::url bpt::guess_url_from_string(std::string_view sv) {
    /// We can probably be a lot smarter about this...
    std::filesystem::path as_path{sv};
    if (not as_path.empty()
        and (as_path.has_root_path()
             or as_path.begin()->filename() == neo::oper::any_of("..", "."))) {
        return neo::url::for_file_path(as_path);
    }

    if (sv.find("://") == sv.npos) {
        std::string s   = "https://" + std::string(sv);
        auto        url = bpt::parse_url(s);
        if (url.host == neo::oper::any_of("localhost", "127.0.0.1", "[::1]")) {
            url.scheme = "http";
        }
        return url;
    }

    auto host = neo::url::host_t::parse(sv);
    if (host.has_value()) {
        std::string s = "https://" + std::string(sv);
        return bpt::parse_url(s);
    }

    auto parsed = neo::url::try_parse(sv);
    if (std::holds_alternative<neo::url>(parsed)) {
        return std::get<neo::url>(parsed);
    }

    if (as_path.begin() != as_path.end()) {
        auto first = as_path.begin()->string();
        host       = neo::url::host_t::parse(first);
        if (host.has_value()) {
            std::string s = "https://" + std::string(sv);
            return bpt::parse_url(s);
        }
    }
    return bpt::parse_url(sv);
}
