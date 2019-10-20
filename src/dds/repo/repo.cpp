#include "./repo.hpp"

#include <dds/sdist.hpp>
#include <dds/util/paths.hpp>

#include <spdlog/spdlog.h>

#include <range/v3/range/conversion.hpp>

using namespace dds;

using namespace ranges;

void repository::_log_blocking(path_ref dirpath) noexcept {
    spdlog::warn("Another process has the repository directory locked [{}]", dirpath.string());
    spdlog::warn("Waiting for repository to be released...");
}

void repository::_init_repo_dir(path_ref dirpath) noexcept {
    fs::create_directories(dirpath / "dist");
}

fs::path repository::default_local_path() noexcept { return dds_data_dir() / "repo"; }

repository repository::open_for_directory(path_ref dirpath) {
    auto dist_dir = dirpath / "dist";
    auto entries  = fs::directory_iterator(dist_dir) | to_vector;
    return {dirpath};
}

void repository::add_sdist(const sdist& sd) {
    auto sd_dest = _root / "dist" / sd.name() / sd.version() / sd.hash();
    if (fs::exists(sd_dest)) {
        spdlog::info("Source distribution '{}' is already available in the local repo",
                     sd.path().string());
        return;
    }
    auto tmp_copy = sd_dest;
    tmp_copy.replace_filename(".tmp-import");
    if (fs::exists(tmp_copy)) {
        fs::remove_all(tmp_copy);
    }
    fs::create_directories(tmp_copy.parent_path());
    fs::copy(sd.path(), tmp_copy, fs::copy_options::recursive);
    fs::rename(tmp_copy, sd_dest);
    spdlog::info("Source distribution '{}' successfully exported", sd.ident());
}
