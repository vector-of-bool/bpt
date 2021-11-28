#include "./url.hpp"

#include <dds/error/try_catch.hpp>

#include <neo/utility.hpp>

#include <filesystem>

using namespace dds;

neo::url dds::guess_url_from_string(std::string_view sv) {
    /// We can probably be a lot smarter about this...
    if (sv.find("://") == sv.npos) {
        std::string s   = "https://" + std::string(sv);
        auto        url = neo::url::parse(s);
        if (url.host == neo::oper::any_of("localhost", "127.0.0.1", "[::1]")) {
            url.scheme = "http";
        }
        return url;
    }

    auto host = neo::url::host_t::parse(sv);
    if (host.has_value()) {
        std::string s = "https://" + std::string(sv);
        return neo::url::parse(s);
    }

    auto parsed = neo::url::try_parse(sv);
    if (std::holds_alternative<neo::url>(parsed)) {
        return std::get<neo::url>(parsed);
    }

    std::filesystem::path as_path{sv};
    if (as_path.begin() != as_path.end()) {
        auto first = as_path.begin()->string();
        host       = neo::url::host_t::parse(first);
        if (host.has_value()) {
            std::string s = "https://" + std::string(sv);
            return neo::url::parse(s);
        }
    }
    return neo::url::parse(sv);
}
