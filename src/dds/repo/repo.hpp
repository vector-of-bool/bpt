#pragma once

#include <dds/sdist.hpp>
#include <dds/util/flock.hpp>
#include <dds/util/fs.hpp>

#include <functional>
#include <optional>
#include <set>
#include <shared_mutex>
#include <vector>

namespace dds {

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
    using sdist_set = std::set<sdist, sdist_compare_t>;

    bool      _write_enabled = false;
    fs::path  _root;
    sdist_set _sdists;

    repository(bool writeable, path_ref p, sdist_set sds)
        : _write_enabled(writeable)
        , _root(p)
        , _sdists(std::move(sds)) {}

    static void       _log_blocking(path_ref dir) noexcept;
    static void       _init_repo_dir(path_ref dir) noexcept;
    static repository _open_for_directory(bool writeable, path_ref);

public:
    template <typename Func>
    static decltype(auto) with_repository(path_ref dirpath, repo_flags flags, Func&& fn) {
        if (!fs::exists(dirpath)) {
            if (flags & repo_flags::create_if_absent) {
                _init_repo_dir(dirpath);
            }
        }

        shared_file_mutex mut{dirpath / ".dds-repo-lock"};
        std::shared_lock  shared_lk{mut, std::defer_lock};
        std::unique_lock  excl_lk{mut, std::defer_lock};

        bool writeable = (flags & repo_flags::write_lock) != repo_flags::none;

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

        auto repo = _open_for_directory(writeable, dirpath);
        return std::invoke((Func &&) fn, std::move(repo));
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

    std::vector<sdist> solve(const std::vector<dependency>& deps) const;
};

}  // namespace dds