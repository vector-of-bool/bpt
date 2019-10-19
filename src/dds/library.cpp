#include <dds/library.hpp>

#include <spdlog/spdlog.h>

using namespace dds;

namespace {

struct pf_info {
    source_list sources;
    fs::path    inc_dir;
    fs::path    src_dir;
};

pf_info collect_pf_sources(path_ref path) {
    auto include_dir = path / "include";
    auto src_dir     = path / "src";

    source_list sources;

    if (fs::exists(include_dir)) {
        if (!fs::is_directory(include_dir)) {
            throw std::runtime_error("The `include` at the root of the project is not a directory");
        }
        auto inc_sources = source_file::collect_for_dir(include_dir);
        // Drop any source files we found within `include/`
        erase_if(sources, [&](auto& info) {
            if (info.kind != source_kind::header) {
                spdlog::warn("Source file in `include` will not be compiled: {}",
                             info.path.string());
                return true;
            }
            return false;
        });
        extend(sources, inc_sources);
    }

    if (fs::exists(src_dir)) {
        if (!fs::is_directory(src_dir)) {
            throw std::runtime_error("The `src` at the root of the project is not a directory");
        }
        auto src_sources = source_file::collect_for_dir(src_dir);
        extend(sources, src_sources);
    }

    return {std::move(sources), include_dir, src_dir};
}

}  // namespace

library library::from_directory(path_ref lib_dir, std::string_view name) {
    auto [sources, inc_dir, src_dir] = collect_pf_sources(lib_dir);

    auto lib = library(lib_dir, name, std::move(sources));

    if (fs::exists(inc_dir)) {
        lib._pub_inc_dir = inc_dir;
        if (fs::exists(src_dir)) {
            lib._priv_inc_dir = src_dir;
        }
    } else {
        lib._pub_inc_dir  = src_dir;
        lib._priv_inc_dir = src_dir;
    }

    return lib;
}
