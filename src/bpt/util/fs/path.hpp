#pragma once

#include <bpt/error/result_fwd.hpp>

#include <filesystem>

namespace dds {

namespace fs = std::filesystem;

/**
 * @brief Alias of a const& to a std::filesystem::path
 */
using path_ref = const fs::path&;

/**
 * @brief An error occurring when resolving a filepath
 */
struct e_resolve_path {
    fs::path value;
};

/**
 * @brief Convert a path to its most-normal form based on dds's path-equivalence.
 *
 * @param p A path to be normalized.
 * @return fs::path The normal form of the path
 *
 * Two paths are "equivalent" (in dds) if they would resolve to the same entity on disk from a given
 * base location. This removes redundant path elements (dots and dot-dots), removes trailing
 * directory separators, and converts the path to it's POSIX (generic) form.
 */
[[nodiscard]] fs::path normalize_path(path_ref p) noexcept;

/**
 * @brief Obtain the normalized absolute path to a possibly-existing file or directory.
 */
[[nodiscard]] fs::path resolve_path_weak(path_ref p) noexcept;

/**
 * @brief Obtain the normalized path to an existing file or directory.
 */
[[nodiscard]] result<fs::path> resolve_path_strong(path_ref p) noexcept;

}  // namespace dds
