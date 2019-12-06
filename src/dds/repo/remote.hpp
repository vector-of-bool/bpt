#pragma once

#include <dds/util/fs.hpp>

#include <dds/sdist.hpp>
#include <dds/temp.hpp>
#include <dds/catalog/git.hpp>
#include <dds/catalog/get.hpp>

#include <libman/library.hpp>
#include <semver/version.hpp>

#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

namespace dds {

struct remote_listing {
    package_id                       pk_id;
    std::variant<git_remote_listing> remote;

    template <typename Func>
    decltype(auto) visit(Func&& fn) const {
        return std::visit(std::forward<Func>(fn), remote);
    }

    temporary_sdist pull_sdist() const;
};

inline constexpr struct remote_listing_compare_t {
    using is_transparent = int;
    bool operator()(const remote_listing& lhs, const remote_listing& rhs) const {
        return lhs.pk_id < rhs.pk_id;
    }
    template <typename Name, typename Version>
    bool operator()(const remote_listing& lhs, const std::tuple<Name, Version>& rhs) const {
        auto&& [name, ver] = rhs;
        return lhs.pk_id < package_id{name, ver};
    }
    template <typename Name, typename Version>
    bool operator()(const std::tuple<Name, Version>& lhs, const remote_listing& rhs) const {
        auto&& [name, ver] = lhs;
        return package_id{name, ver} < rhs.pk_id;
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