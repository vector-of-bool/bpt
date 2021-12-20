#include "./pkg_id.hpp"

#include <boost/leaf/exception.hpp>
#include <neo/ufmt.hpp>

#include <charconv>
#include <dds/pkg/id.hpp>

using namespace dds;
using namespace dds::crs;

crs::pkg_id crs::pkg_id::parse_str(std::string_view sv) {
    auto tilde        = sv.find("~");
    int  meta_version = 0;
    if (tilde != sv.npos) {
        auto tail   = sv.substr(tilde + 1);
        auto fc_res = std::from_chars(tail.data(), tail.data() + tail.size(), meta_version);
        if (fc_res.ec != std::errc{}) {
            BOOST_LEAF_THROW_EXCEPTION(e_invalid_pkg_id_str{std::string(sv)});
        }
        sv = sv.substr(0, tilde);
    }
    auto legacy = dds::pkg_id::parse(sv);
    return crs::pkg_id{legacy.name, legacy.version, meta_version};
}

std::string crs::pkg_id::to_string() const noexcept {
    return neo::ufmt("{}@{}~{}", name.str, version.to_string(), meta_version);
}
