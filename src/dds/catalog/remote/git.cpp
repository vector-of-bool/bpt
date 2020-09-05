#include "./git.hpp"

#include <dds/error/errors.hpp>
#include <dds/proc.hpp>
#include <dds/util/log.hpp>

#include <nlohmann/json.hpp>

void dds::git_remote_listing::pull_to(const dds::package_id& pid, dds::path_ref dest) const {
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

        auto pkg_strm         = dds::open(dest / "package.json5", std::ios::binary | std::ios::out);
        auto man_json         = nlohmann::json::object();
        man_json["name"]      = pid.name;
        man_json["version"]   = pid.version.to_string();
        man_json["namespace"] = auto_lib->namespace_;
        pkg_strm << nlohmann::to_string(man_json);

        auto lib_strm    = dds::open(dest / "library.json5", std::ios::binary | std::ios::out);
        auto lib_json    = nlohmann::json::object();
        lib_json["name"] = auto_lib->name;
        lib_strm << nlohmann::to_string(lib_json);
    }
}
