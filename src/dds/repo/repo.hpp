#pragma once

#include <dds/util/flock.hpp>
#include <dds/util/fs.hpp>

#include <cassert>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <vector>

namespace dds {

struct sdist;

enum repo_flags {
    none             = 0b00,
    read             = none,
    create_if_absent = 0b01,
    write_lock       = 0b10,
};

enum class if_exists {
    replace,
    throw_exc,
    ignore,
};

inline repo_flags operator|(repo_flags a, repo_flags b) {
    return static_cast<repo_flags>(int(a) | int(b));
}

class repository {
    fs::path _root;

    repository(path_ref p)
        : _root(p) {}

    static void _log_blocking(path_ref dir) noexcept;
    static void _init_repo_dir(path_ref dir) noexcept;

    fs::path _dist_dir() const noexcept { return _root / "dist"; }

public:
    template <typename Func>
    static decltype(auto) with_repository(path_ref dirpath, repo_flags flags, Func&& fn) {
        if (!fs::exists(dirpath)) {
            if (flags & repo_flags::create_if_absent) {
                _init_repo_dir(dirpath);
            }
        }

        shared_file_mutex mut{dirpath / ".lock"};
        std::shared_lock  shared_lk{mut, std::defer_lock};
        std::unique_lock  excl_lk{mut, std::defer_lock};

        if (flags & repo_flags::write_lock) {
            if (!excl_lk.try_lock()) {
                _log_blocking(dirpath);
                excl_lk.lock();
            }
        } else {
            if (!shared_lk.try_lock()) {
                _log_blocking(dirpath);
                shared_lk.lock();
            }
        }

        return std::invoke((Func &&) fn, open_for_directory(dirpath));
    }

    static repository open_for_directory(path_ref);

    static fs::path default_local_path() noexcept;

    void                 add_sdist(const sdist&, if_exists = if_exists::throw_exc);
    std::optional<sdist> get_sdist(std::string_view name, std::string_view version) const;
    std::vector<sdist>   load_sdists() const noexcept;
};

}  // namespace dds