#include "./git.hpp"

#include <dds/error/errors.hpp>
#include <dds/proc.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>

#include <neo/url.hpp>
#include <neo/url/query.hpp>

using namespace dds;
using namespace std::literals;

git_remote_pkg git_remote_pkg::from_url(const neo::url& url) {
    if (!url.fragment) {
        BOOST_LEAF_THROW_EXCEPTION(
            user_error<errc::invalid_remote_url>(
                "Git URL requires a fragment specified the Git ref to clone"),
            DDS_E_ARG(e_url_string{url.to_string()}));
    }
    git_remote_pkg ret;
    ret.url = url;
    if (url.scheme.starts_with("git+")) {
        ret.url.scheme = url.scheme.substr(4);
    } else if (url.scheme.ends_with("+git")) {
        ret.url.scheme = url.scheme.substr(0, url.scheme.size() - 4);
    } else {
        // Leave the URL as-is
    }
    ret.ref = *url.fragment;
    ret.url.fragment.reset();
    return ret;
}

neo::url git_remote_pkg::do_to_url() const {
    neo::url ret = url;
    ret.fragment = ref;
    if (ret.scheme != "git") {
        ret.scheme = "git+" + ret.scheme;
    }
    return ret;
}

void git_remote_pkg::do_get_raw(path_ref dest) const {
    fs::remove(dest);
    dds_log(info, "Clone Git repository [{}] (at {}) to [{}]", url.to_string(), ref, dest.string());
    auto command
        = {"git"s, "clone"s, "--depth=1"s, "--branch"s, ref, url.to_string(), dest.string()};
    auto git_res = run_proc(command);
    if (!git_res.okay()) {
        BOOST_LEAF_THROW_EXCEPTION(
            make_external_error<errc::git_clone_failure>(
                "Git clone operation failed [Git command: {}] [Exitted {}]:\n{}",
                quote_command(command),
                git_res.retc,
                git_res.output),
            url);
    }
}
