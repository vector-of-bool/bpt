#pragma once

#include <bpt/util/fs/path.hpp>

#include <neo/iterator_facade.hpp>

#include <string_view>
#include <vector>

namespace dds {

namespace detail {

struct glob_impl;

struct glob_iter_state;

}  // namespace detail

class glob;

class glob_iterator : public neo::iterator_facade<glob_iterator> {
    std::shared_ptr<const detail::glob_impl> _impl;

    std::shared_ptr<detail::glob_iter_state> _state;

    bool _done = true;

public:
    glob_iterator() = default;
    glob_iterator(glob impl, path_ref root);

    fs::directory_entry dereference() const noexcept;
    void                increment();

    struct sentinel_type {};

    bool operator==(sentinel_type) const noexcept { return at_end(); }
    bool at_end() const noexcept { return _done; }

    glob_iterator begin() const noexcept { return *this; }
    auto          end() const noexcept { return sentinel_type{}; }
};

class glob {
    friend class glob_iterator;
    std::shared_ptr<const detail::glob_impl> _impl;

    glob() = default;

public:
    static glob compile(std::string_view str);

    auto scan_from(path_ref root) const noexcept { return glob_iterator(*this, root); }

    auto begin() const noexcept { return scan_from(fs::current_path()); }
    auto end() const noexcept { return glob_iterator::sentinel_type{}; }

    bool match(path_ref) const noexcept;

    std::string_view string() const noexcept;
};

}  // namespace dds
