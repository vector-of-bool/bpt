#pragma once

#include "./path.hpp"

#include <dds/error/result_fwd.hpp>

#include <ostream>

namespace dds::inline file_utils {

struct e_remove_file {
    fs::path value;
};

/**
 * @brief Ensure that the named file/directory does not exist.
 *
 * If the file does not exist, no error occurs.
 */
[[nodiscard]] result<void> ensure_absent(path_ref path) noexcept;

/**
 * @brief Delete the named file/directory.
 *
 * If the file does not exist, results in an error.
 */
[[nodiscard]] result<void> remove_file(path_ref file) noexcept;

/**
 * @brief Move the file or directory 'source' to 'dest'
 *
 * @param source The file or directory to move
 * @param dest The destination path (not parent directory!) of the file/directory
 */
[[nodiscard]] result<void> move_file(path_ref source, path_ref dest);

struct e_copy_file {
    fs::path source;
    fs::path dest;

    friend std::ostream& operator<<(std::ostream& out, const e_copy_file& self) noexcept {
        out << "e_copy_file: From [" << self.source.string() << "] to [" << self.dest.string()
            << "]";
        return out;
    }
};

struct e_move_file {
    fs::path source;
    fs::path dest;

    friend std::ostream& operator<<(std::ostream& out, const e_move_file& self) noexcept {
        out << "e_move_file: From [" << self.source.string() << "] to [" << self.dest.string()
            << "]";
        return out;
    }
};

struct e_symlink {
    fs::path symlink;
    fs::path target;

    friend std::ostream& operator<<(std::ostream& out, const e_symlink& self) noexcept {
        out << "e_symlink: Symlink [" << self.symlink.string() << "] -> [" << self.target.string()
            << "]";
        return out;
    }
};

/**
 * @brief Copy the given file or directory to 'dest'
 *
 * @param source The file to copy
 * @param dest The destination of the copied file (not the parent directory!)
 * @param opts Options for the copy operation
 */
[[nodiscard]] result<void>
copy_file(path_ref source, path_ref dest, fs::copy_options opts = {}) noexcept;

struct e_copy_tree {
    fs::path source;
    fs::path dest;

    friend std::ostream& operator<<(std::ostream& out, const e_copy_tree& self) noexcept {
        out << "e_symlink: From [" << self.source.string() << "] to [" << self.dest.string() << "]";
        return out;
    }
};

[[nodiscard]] result<void>
copy_tree(path_ref source, path_ref dest, fs::copy_options opts = {}) noexcept;

/**
 * @brief Create a symbolic link at 'symlink' that points to 'target'
 *
 * @param target The target of the symlink
 * @param symlink The path to the symlink object
 */
[[nodiscard]] result<void> create_symlink(path_ref target, path_ref symlink) noexcept;

}  // namespace dds::inline file_utils
