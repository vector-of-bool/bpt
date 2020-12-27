#include "./listing.hpp"

#include "./get/dds_http.hpp"
#include "./get/git.hpp"
#include "./get/github.hpp"
#include "./get/http.hpp"

#include <dds/error/errors.hpp>
#include <dds/util/result.hpp>
#include <dds/util/string.hpp>

#include <neo/url.hpp>
#include <neo/utility.hpp>
#include <range/v3/distance.hpp>

using namespace dds;

any_remote_pkg::~any_remote_pkg() = default;
any_remote_pkg::any_remote_pkg() {}

static std::shared_ptr<remote_pkg_base> do_parse_url(const neo::url& url) {
    if (url.scheme == neo::oper::any_of("http", "https")) {
        return std::make_shared<http_remote_pkg>(http_remote_pkg::from_url(url));
    } else if (url.scheme
               == neo::oper::any_of("git", "git+https", "git+http", "https+git", "http+git")) {
        return std::make_shared<git_remote_pkg>(git_remote_pkg::from_url(url));
    } else if (url.scheme == "github") {
        return std::make_shared<github_remote_pkg>(github_remote_pkg::from_url(url));
    } else if (url.scheme == neo::oper::any_of("dds+http", "http+dds", "dds+https", "https+dds")) {
        return std::make_shared<dds_http_remote_pkg>(dds_http_remote_pkg::from_url(url));
    } else {
        BOOST_LEAF_THROW_EXCEPTION(make_user_error<errc::invalid_remote_url>(
                                       "Unknown scheme '{}' for remote package listing URL",
                                       url.scheme),
                                   url);
    }
}

any_remote_pkg any_remote_pkg::from_url(const neo::url& url) {
    auto ptr = do_parse_url(url);
    return any_remote_pkg(ptr);
}

neo::url any_remote_pkg::to_url() const {
    neo_assert(expects, !!_impl, "Accessing an inactive any_remote_pkg");
    return _impl->to_url();
}

std::string any_remote_pkg::to_url_string() const { return to_url().to_string(); }

void any_remote_pkg::get_sdist(path_ref dest) const {
    neo_assert(expects, !!_impl, "Accessing an inactive any_remote_pkg");
    _impl->get_sdist(dest);
}
