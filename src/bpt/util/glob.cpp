#include "./glob.hpp"

#include "./fnmatch.hpp"

#include <neo/assert.hpp>

#include <optional>

namespace {

enum glob_coro_ret {
    reenter_again,
    yield_value,
    done,
};

}  // namespace

namespace dds::detail {

struct rglob_item {
    std::optional<dds::fnmatch::pattern> pattern;
};

struct glob_impl {
    std::string             spelling;
    std::vector<rglob_item> items;
};

struct glob_iter_state {
    fs::path                                root;
    const glob_impl&                        impl;
    std::vector<rglob_item>::const_iterator pat_iter = impl.items.begin();

    const bool is_leaf_pattern = std::next(pat_iter) == impl.items.end();

    fs::directory_entry    entry{};
    fs::directory_iterator dir_iter{root};
    const bool             is_rglob = !pat_iter->pattern.has_value();

    std::unique_ptr<glob_iter_state> _next_state{};
    int                              _state_label = 0;

    fs::directory_entry get_entry() const noexcept {
        if (_next_state) {
            return _next_state->get_entry();
        }
        return entry;
    }

#define CORO_REENTER_POINT                                                                         \
    case __LINE__:                                                                                 \
        static_assert(true)
#define CORO_SAVE_POINT _state_label = __LINE__

#define YIELD(E)                                                                                   \
    do {                                                                                           \
        CORO_SAVE_POINT;                                                                           \
        entry = E;                                                                                 \
        return yield_value;                                                                        \
    } while (0);                                                                                   \
    CORO_REENTER_POINT

#define EXIT_DIRECTORY()                                                                           \
    do {                                                                                           \
        return done;                                                                               \
    } while (0);                                                                                   \
    CORO_REENTER_POINT

#define ENTER_DIRECTORY(D, Pat)                                                                    \
    do {                                                                                           \
        _next_state.reset(new glob_iter_state{fs::path(D), impl, Pat});                            \
        CORO_SAVE_POINT;                                                                           \
        return reenter_again;                                                                      \
    } while (0);                                                                                   \
    CORO_REENTER_POINT

#define CONTINUE()                                                                                 \
    do {                                                                                           \
        _state_label = 0;                                                                          \
        return reenter_again;                                                                      \
    } while (0)

    glob_coro_ret reenter() {
        if (_next_state) {
            auto st = _next_state->reenter();
            if (st == done) {
                _next_state.reset();
                return reenter_again;
            }
            return st;
        }

        const bool dir_done     = dir_iter == fs::directory_iterator();
        const auto cur_pattern  = pat_iter->pattern;
        const bool cur_is_rglob = !cur_pattern.has_value();

        switch (_state_label) {
        case 0:
            //
            if (dir_done) {
                EXIT_DIRECTORY();
            }
            entry = *dir_iter++;

            if (cur_is_rglob) {
                if (is_leaf_pattern) {
                    YIELD(entry);
                } else if (std::next(pat_iter)->pattern.value().match(
                               fs::path(entry).filename().string())) {
                    // The next pattern in the glob will match this file directly.
                    if (entry.is_directory()) {
                        ENTER_DIRECTORY(entry, std::next(pat_iter));
                    } else {
                        YIELD(entry);
                    }
                }
                if (entry.is_directory()) {
                    ENTER_DIRECTORY(entry, pat_iter);
                } else {
                    // A non-directory file matches an `**` pattern? Ignore it.
                }
            } else {
                if (cur_pattern->match(fs::path(entry).filename().string())) {
                    // We match this entry
                    if (is_leaf_pattern) {
                        YIELD(entry);
                    } else if (entry.is_directory()) {
                        ENTER_DIRECTORY(entry, std::next(pat_iter));
                    }
                }
            }
        }

        CONTINUE();
    }
};  // namespace dds::detail

}  // namespace dds::detail

namespace {

dds::detail::glob_impl compile_glob_expr(std::string_view pattern) {
    using namespace dds::detail;

    glob_impl acc{};
    acc.spelling = std::string(pattern);

    while (!pattern.empty()) {
        const auto next_slash = pattern.find('/');
        const auto next_part  = pattern.substr(0, next_slash);
        if (next_slash != pattern.npos) {
            pattern.remove_prefix(next_slash + 1);
        } else {
            pattern = "";
        }

        if (next_part == "**") {
            acc.items.emplace_back();
        } else {
            acc.items.push_back({dds::fnmatch::compile(next_part)});
        }
    }

    if (acc.items.empty()) {
        throw std::runtime_error("Invalid path glob expression (Must not be empty!)");
    }

    return acc;
}

}  // namespace

dds::glob_iterator::glob_iterator(dds::glob gl, dds::path_ref root)
    : _impl(gl._impl)
    , _done(false) {

    _state = std::make_shared<detail::glob_iter_state>(detail::glob_iter_state{root, *_impl});
    increment();
}

void dds::glob_iterator::increment() {
    auto st = reenter_again;
    while (st == reenter_again) {
        st = _state->reenter();
    }
    _done = st == done;
}

dds::fs::directory_entry dds::glob_iterator::dereference() const noexcept {
    return _state->get_entry();
}

dds::glob dds::glob::compile(std::string_view pattern) {
    glob ret;
    ret._impl = std::make_shared<dds::detail::glob_impl>(compile_glob_expr(pattern));
    return ret;
}

namespace {

using path_iter = dds::fs::path::const_iterator;
using pat_iter  = std::vector<dds::detail::rglob_item>::const_iterator;

bool check_matches(path_iter       elem_it,
                   const path_iter elem_stop,
                   pat_iter        pat_it,
                   const pat_iter  pat_stop) noexcept {
    if (elem_it == elem_stop && pat_it == pat_stop) {
        return true;
    }
    if (elem_it == elem_stop || pat_it == pat_stop) {
        return false;
    }
    if (pat_it->pattern.has_value()) {
        // A regular pattern
        if (!pat_it->pattern->match(elem_it->string())) {
            return false;
        }
        return check_matches(++elem_it, elem_stop, ++pat_it, pat_stop);
    } else {
        // An rglob pattern "**". Check by peeling of individual path elements
        const auto next_pat = std::next(pat_it);
        if (next_pat == pat_stop) {
            // The "**" is at the end of the glob. This matches everything.
            return true;
        }
        for (; elem_it != elem_stop; ++elem_it) {
            if (check_matches(elem_it, elem_stop, next_pat, pat_stop)) {
                return true;
            }
        }
        return false;
    }
}

}  // namespace

bool dds::glob::match(dds::path_ref filepath) const noexcept {
    return check_matches(filepath.begin(),
                         filepath.end(),
                         _impl->items.cbegin(),
                         _impl->items.cend());
}

std::string_view dds::glob::string() const noexcept { return _impl->spelling; }
