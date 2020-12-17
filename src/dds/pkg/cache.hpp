#pragma once

#include <dds/pkg/db.hpp>
#include <dds/source/dist.hpp>
#include <dds/util/flock.hpp>
#include <dds/util/fs.hpp>

#include <neo/fwd.hpp>

#include <functional>
#include <optional>
#include <set>
#include <shared_mutex>
#include <vector>

namespace dds {

enum pkg_cache_flags {
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

inline pkg_cache_flags operator|(pkg_cache_flags a, pkg_cache_flags b) {
    return static_cast<pkg_cache_flags>(int(a) | int(b));
}

class pkg_cache {
    using sdist_set = std::set<sdist, sdist_compare_t>;

    bool      _write_enabled = false;
    fs::path  _root;
    sdist_set _sdists;

    pkg_cache(bool writeable, path_ref p, sdist_set sds)
        : _write_enabled(writeable)
        , _root(p)
        , _sdists(std::move(sds)) {}

    static void      _log_blocking(path_ref dir) noexcept;
    static void      _init_cache_dir(path_ref dir) noexcept;
    static pkg_cache _open_for_directory(bool writeable, path_ref);

public:
    template <typename Func>
    static decltype(auto) with_cache(path_ref dirpath, pkg_cache_flags flags, Func&& fn) {
        if (!fs::exists(dirpath)) {
            if (flags & pkg_cache_flags::create_if_absent) {
                _init_cache_dir(dirpath);
            }
        }

        shared_file_mutex mut{dirpath / ".dds-cache-lock"};
        std::shared_lock  shared_lk{mut, std::defer_lock};
        std::unique_lock  excl_lk{mut, std::defer_lock};

        bool writeable = (flags & pkg_cache_flags::write_lock) != pkg_cache_flags::none;

        if (writeable) {
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

        auto cache = _open_for_directory(writeable, dirpath);
        return std::invoke(NEO_FWD(fn), std::move(cache));
    }

    static fs::path default_local_path() noexcept;

    void add_sdist(const sdist&, if_exists = if_exists::throw_exc);

    const sdist* find(const package_id& pk) const noexcept;

    auto iter_sdists() const noexcept {
        class ret {
            const sdist_set& s;

        public:
            ret(const sdist_set& s)
                : s(s) {}

            auto begin() const { return s.cbegin(); }
            auto end() const { return s.cend(); }
        } r{_sdists};
        return r;
    }

    std::vector<package_id> solve(const std::vector<dependency>& deps, const pkg_db&) const;
};

}  // namespace dds