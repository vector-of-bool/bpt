#include "./info.hpp"

#include <dds/error/errors.hpp>
#include <dds/util/string.hpp>

#include <neo/url.hpp>
#include <neo/utility.hpp>

using namespace dds;

dds::remote_listing_var dds::parse_remote_url(std::string_view sv) {
    neo_assertion_breadcrumbs("Loading package remote from URI string", sv);
    auto url = neo::url::parse(sv);
    if (url.scheme == neo::oper::any_of("git+https", "git+http", "http+git", "https+git", "git")) {
        return git_remote_listing::from_url(sv);
    } else if (url.scheme == neo::oper::any_of("http", "https")) {
        return http_remote_listing::from_url(sv);
    } else if (url.scheme == neo::oper::any_of("dds+http", "dds+https", "http+dds", "https+dds")) {
        fs::path path         = url.path;
        auto     leaf         = path.filename().string();
        auto     namever_path = replace(leaf, "@", "/");
        url.path = (path.parent_path() / "pkg" / namever_path / "sdist.tar.gz").generic_string();
        return http_remote_listing::from_url(url.to_string());
    } else {
        throw_user_error<
            errc::invalid_remote_url>("Unknown scheme '{}' for remote package URL '{}'",
                                      url.scheme,
                                      sv);
    }
}
