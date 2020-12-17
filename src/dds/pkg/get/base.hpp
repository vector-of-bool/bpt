#pragma once

#include <libman/package.hpp>
#include <neo/concepts.hpp>

#include <optional>
#include <vector>

namespace dds {

struct pkg_id;

struct remote_listing_base {
    std::optional<lm::usage> auto_lib{};

    void generate_auto_lib_files(const pkg_id& pid, path_ref root) const;
};

template <typename T>
concept remote_listing = neo::derived_from<std::remove_cvref_t<T>, remote_listing_base>;

}  // namespace dds
