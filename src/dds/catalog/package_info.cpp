#include "./package_info.hpp"

#include <dds/error/errors.hpp>

#include <neo/url.hpp>

using namespace dds;

dds::remote_listing_var dds::parse_remote_url(std::string_view sv) {
    auto url = neo::url::parse(sv);
    if (url.scheme == "git+https" || url.scheme == "git+http" || url.scheme == "https+git"
        || url.scheme == "http+git" || url.scheme == "git") {
        return git_remote_listing::from_url(sv);
    } else {
        throw_user_error<
            errc::invalid_remote_url>("Unknown scheme '{}' for remote package URL '{}'",
                                      url.scheme,
                                      sv);
    }
}
