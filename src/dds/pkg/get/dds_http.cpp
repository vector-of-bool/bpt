#include "./dds_http.hpp"

#include "./http.hpp"

#include <fmt/core.h>

using namespace dds;

neo::url dds_http_remote_pkg::do_to_url() const {
    auto ret   = repo_url;
    ret.scheme = "dds+" + ret.scheme;
    ret.path   = fmt::format("{}/{}", ret.path, pkg_id.to_string());
    return ret;
}

dds_http_remote_pkg dds_http_remote_pkg::from_url(const neo::url& url) {
    auto repo_url = url;
    if (repo_url.scheme.starts_with("dds+")) {
        repo_url.scheme = repo_url.scheme.substr(4);
    } else if (repo_url.scheme.ends_with("+dds")) {
        repo_url.scheme = repo_url.scheme.substr(0, repo_url.scheme.size() - 4);
    } else {
        // Nothing to trim
    }

    fs::path full_path = repo_url.path;
    repo_url.path      = full_path.parent_path().generic_string();
    auto pkg_id        = dds::pkg_id::parse(full_path.filename().string());

    return {repo_url, pkg_id};
}

void dds_http_remote_pkg::do_get_raw(path_ref dest) const {
    auto     http_url = repo_url;
    fs::path path     = fs::path(repo_url.path) / "pkg" / pkg_id.name / pkg_id.version.to_string()
        / "sdist.tar.gz";
    http_url.path = path.lexically_normal().generic_string();
    http_remote_pkg http;
    http.url = http_url;
    http.get_raw_directory(dest);
}
