#include "./git.hpp"

#include <dds/error/errors.hpp>
#include <dds/proc.hpp>
#include <dds/util/log.hpp>

#include <neo/url.hpp>
#include <neo/url/query.hpp>
#include <nlohmann/json.hpp>

using namespace dds;

void git_remote_listing::pull_to(const package_id& pid, path_ref dest) const {
    fs::remove_all(dest);
    using namespace std::literals;
    dds_log(info, "Clone Git repository [{}] (at {}) to [{}]", url, ref, dest.string());
    auto command = {"git"s, "clone"s, "--depth=1"s, "--branch"s, ref, url, dest.generic_string()};
    auto git_res = run_proc(command);
    if (!git_res.okay()) {
        throw_external_error<errc::git_clone_failure>(
            "Git clone operation failed [Git command: {}] [Exitted {}]:\n{}",
            quote_command(command),
            git_res.retc,
            git_res.output);
    }

    for (const auto& tr : transforms) {
        tr.apply_to(dest);
    }

    if (auto_lib.has_value()) {
        dds_log(info, "Generating library data automatically");

        auto pkg_strm         = open(dest / "package.json5", std::ios::binary | std::ios::out);
        auto man_json         = nlohmann::json::object();
        man_json["name"]      = pid.name;
        man_json["version"]   = pid.version.to_string();
        man_json["namespace"] = auto_lib->namespace_;
        pkg_strm << nlohmann::to_string(man_json);

        auto lib_strm    = open(dest / "library.json5", std::ios::binary | std::ios::out);
        auto lib_json    = nlohmann::json::object();
        lib_json["name"] = auto_lib->name;
        lib_strm << nlohmann::to_string(lib_json);
    }
}

git_remote_listing git_remote_listing::from_url(std::string_view sv) {
    auto url = neo::url::parse(sv);
    dds_log(trace, "Create Git remote listing from URL '{}'", sv);

    auto ref     = url.fragment;
    url.fragment = {};
    auto q       = url.query;
    url.query    = {};

    std::optional<lm::usage> auto_lib;

    if (url.scheme.starts_with("git+")) {
        url.scheme = url.scheme.substr(4);
    } else if (url.scheme.ends_with("+git")) {
        url.scheme = url.scheme.substr(0, url.scheme.size() - 4);
    } else {
        // Leave the URL as-is
    }

    if (q) {
        neo::basic_query_string_view qsv{*q};
        for (auto qstr : qsv) {
            if (qstr.key_raw() != "lm") {
                dds_log(warn, "Unknown query string parameter in package url: '{}'", qstr.string());
            } else {
                auto_lib = lm::split_usage_string(qstr.value_decoded());
            }
        }
    }

    if (!ref) {
        throw_user_error<errc::invalid_remote_url>(
            "Git URL requires a fragment specifying the Git ref to clone");
    }
    return {.url = url.to_string(), .ref = *ref, .auto_lib = auto_lib, .transforms = {}};
}
