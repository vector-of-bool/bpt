#include "./github.hpp"

#include "./http.hpp"

#include <dds/error/errors.hpp>
#include <dds/util/result.hpp>

#include <fmt/format.h>
#include <range/v3/iterator/operations.hpp>

using namespace dds;

neo::url github_remote_pkg::do_to_url() const {
    neo::url ret;
    ret.scheme = "github";
    ret.path   = fmt::format("{}/{}/{}", owner, reponame, ref);
    return ret;
}

void github_remote_pkg::do_get_raw(path_ref dest) const {
    http_remote_pkg http;
    auto new_url = fmt::format("https://github.com/{}/{}/archive/{}.tar.gz", owner, reponame, ref);
    http.url     = neo::url::parse(new_url);
    http.strip_n_components = 1;
    http.get_raw_directory(dest);
}

github_remote_pkg github_remote_pkg::from_url(const neo::url& url) {
    fs::path path = url.path;
    if (ranges::distance(path) != 3) {
        BOOST_LEAF_THROW_EXCEPTION(make_user_error<errc::invalid_remote_url>(
                                       "'github:' URLs should have a path with three segments"),
                                   url);
    }
    github_remote_pkg ret;
    // Split the three path elements as {owner}/{reponame}/{git-ref}
    auto elem_iter = path.begin();
    ret.owner      = (*elem_iter++).generic_string();
    ret.reponame   = (*elem_iter++).generic_string();
    ret.ref        = (*elem_iter).generic_string();
    return ret;
}
