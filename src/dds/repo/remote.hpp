#pragma once

#include <dds/util/fs.hpp>

#include <dds/sdist.hpp>
#include <dds/temp.hpp>
#include <libman/library.hpp>
#include <semver/version.hpp>

#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

namespace dds {

struct temporary_sdist {
    temporary_dir tmpdir;
    dds::sdist    sdist;
};

struct git_remote_listing {
    std::string              url;
    std::string              ref;
    std::optional<lm::usage> auto_lib;

    void clone(path_ref path) const;
};

struct remote_listing {
    std::string                      name;
    semver::version                  version;
    std::variant<git_remote_listing> remote;

    template <typename Func>
    decltype(auto) visit(Func&& fn) const {
        return std::visit(std::forward<Func>(fn), remote);
    }

    temporary_sdist pull_sdist() const;
};

inline constexpr struct remote_listing_compare_t {
    using is_transparent = int;
    auto tie(const remote_listing& rl) const { return std::tie(rl.name, rl.version); }
    bool operator()(const remote_listing& lhs, const remote_listing& rhs) const {
        return tie(lhs) < tie(rhs);
    }
    template <typename Name, typename Version>
    bool operator()(const remote_listing& lhs, const std::tuple<Name, Version>& rhs) const {
        return tie(lhs) < rhs;
    }
    template <typename Name, typename Version>
    bool operator()(const std::tuple<Name, Version>& lhs, const remote_listing& rhs) const {
        return lhs < tie(rhs);
    }
} remote_listing_compare;

class remote_directory {
    using listing_set = std::set<remote_listing, remote_listing_compare_t>;
    listing_set _remotes;

    remote_directory(listing_set s)
        : _remotes(std::move(s)) {}

public:
    static remote_directory load_from_file(path_ref);

    void ensure_all_local(const class repository& repo) const;

    const remote_listing* find(std::string_view name, semver::version ver) const noexcept;
};

}  // namespace dds