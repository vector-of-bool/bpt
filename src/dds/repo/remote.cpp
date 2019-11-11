#include "./remote.hpp"

#include <dds/deps.hpp>
#include <dds/proc.hpp>
#include <dds/repo/repo.hpp>
#include <dds/sdist.hpp>
#include <dds/temp.hpp>
#include <dds/toolchain/toolchain.hpp>

#include <spdlog/spdlog.h>

#include <libman/parse.hpp>

#include <algorithm>

using namespace dds;

namespace {

struct read_listing_item {
    std::string_view                                    _key;
    std::set<remote_listing, remote_listing_compare_t>& out;

    bool operator()(std::string_view context, std::string_view key, std::string_view value) {
        if (key != _key) {
            return false;
        }

        auto nested        = lm::nested_kvlist::parse(value);
        auto name_ver_pair = split_shell_string(nested.primary);
        if (name_ver_pair.size() != 2) {
            throw std::runtime_error(
                fmt::format("{}: Invalid Remote-Package identity: '{}'", context, nested.primary));
        }
        auto name    = name_ver_pair[0];
        auto version = semver::version::parse(name_ver_pair[1]);
        put_listing(context, name, version, nested.pairs);
        return true;
    }

    void put_listing(std::string_view     context,
                     std::string          name,
                     semver::version      version,
                     const lm::pair_list& pairs) {
        if (pairs.find("git")) {
            std::string              url;
            std::string              ref;
            std::optional<lm::usage> auto_id;
            lm::read(fmt::format("{}: Parsing Git remote listing", context),
                     pairs,
                     lm::read_required("url", url),
                     lm::read_required("ref", ref),
                     lm::read_check_eq("git", ""),
                     lm::read_opt("auto", auto_id, &lm::split_usage_string),
                     lm::reject_unknown());
            auto did_insert = out.emplace(remote_listing{std::move(name),
                                                         version,
                                                         git_remote_listing{url, ref, auto_id}})
                                  .second;
            if (!did_insert) {
                spdlog::warn("Duplicate remote package defintion for {} {}",
                             name,
                             version.to_string());
            }
        } else {
            throw std::runtime_error(fmt::format("Unable to determine remote type of package {} {}",
                                                 name,
                                                 version.to_string()));
        }
    }
};

temporary_sdist do_pull_sdist(const remote_listing& listing, const git_remote_listing& git) {
    auto tmpdir = dds::temporary_dir::create();
    using namespace std::literals;
    spdlog::info("Cloning repository: {} [{}] ...", git.url, git.ref);
    auto command = {"git"s,
                    "clone"s,
                    "--depth=1"s,
                    "--branch"s,
                    git.ref,
                    git.url,
                    tmpdir.path().generic_string()};
    auto git_res = run_proc(command);
    if (!git_res.okay()) {
        throw std::runtime_error(
            fmt::format("Git clone operation failed [Git command: {}] [Exitted {}]:\n{}",
                        quote_command(command),
                        git_res.retc,
                        git_res.output));
    }
    spdlog::info("Create sdist from clone ...");
    if (git.auto_lib.has_value()) {
        spdlog::info("Generating library data automatically");
        auto pkg_strm = dds::open(tmpdir.path() / "package.dds", std::ios::binary | std::ios::out);
        pkg_strm << "Name: " << listing.name << '\n'                    //
                 << "Version: " << listing.version.to_string() << '\n'  //
                 << "Namespace: " << git.auto_lib->namespace_;
        auto lib_strm = dds::open(tmpdir.path() / "library.dds", std::ios::binary | std::ios::out);
        lib_strm << "Name: " << git.auto_lib->name;
    }

    sdist_params params;
    params.project_dir = tmpdir.path();
    auto sd_tmp_dir    = dds::temporary_dir::create();
    params.dest_path   = sd_tmp_dir.path();
    params.force       = true;
    auto sd            = create_sdist(params);
    return {sd_tmp_dir, sd};
}

}  // namespace

temporary_sdist remote_listing::pull_sdist() const {
    auto tsd = visit([&](auto&& actual) { return do_pull_sdist(*this, actual); });
    if (tsd.sdist.manifest.name != name) {
        throw std::runtime_error(
            fmt::format("The name in the generated sdist ('{}') does not match the name listed in "
                        "the remote listing file (expected '{}')",
                        tsd.sdist.manifest.name,
                        name));
    }
    if (tsd.sdist.manifest.version != version) {
        throw std::runtime_error(
            fmt::format("The version of the generated sdist is '{}', which does not match the "
                        "expected version '{}'",
                        tsd.sdist.manifest.version.to_string(),
                        version.to_string()));
    }
    return tsd;
}

remote_directory remote_directory::load_from_file(path_ref filepath) {
    auto        kvs = lm::parse_file(filepath);
    listing_set listings;
    lm::read(fmt::format("Loading remote package listing from {}", filepath.string()),
             kvs,
             read_listing_item{"Remote-Package", listings},
             lm::reject_unknown());
    return {std::move(listings)};
}

const remote_listing* remote_directory::find(std::string_view name, semver::version ver) const
    noexcept {
    auto found = _remotes.find(std::tie(name, ver));
    if (found == _remotes.end()) {
        return nullptr;
    }
    return &*found;
}

void remote_directory::ensure_all_local(const repository&) const {
    spdlog::critical("Dependency download is not fully implemented!");
}
