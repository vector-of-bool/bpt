#pragma once

#include <range/v3/algorithm/min_element.hpp>
#include <range/v3/view/all.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace dds {

std::size_t lev_edit_distance(std::string_view a, std::string_view b) noexcept;

struct e_did_you_mean {
    std::optional<std::string> value;

    void log_as_error() const noexcept;
};

class dym_target {
    std::optional<std::string>      _candidate;
    dym_target*                     _tls_prev = nullptr;
    static thread_local dym_target* _tls_current;

public:
    dym_target()
        : _tls_prev(_tls_current) {
        _tls_current = this;
    }
    dym_target(const dym_target&) = delete;
    ~dym_target() { _tls_current = _tls_prev; }

    template <typename Func>
    static void fill(Func&& fn) noexcept {
        if (_tls_current) {
            _tls_current->_candidate = fn();
        }
    }

    auto& candidate() const noexcept { return _candidate; }

    auto e_value() const noexcept { return e_did_you_mean{_candidate}; }

    std::string sentence_suffix() const noexcept {
        if (_candidate) {
            return " (Did you mean '" + *_candidate + "'?)";
        }
        return "";
    }
};

template <typename Range>
std::optional<std::string> did_you_mean(std::string_view given, Range&& strings) noexcept {
    auto cand = ranges::min_element(strings, ranges::less{}, [&](std::string_view candidate) {
        return lev_edit_distance(candidate, given);
    });
    if (cand == ranges::end(strings)) {
        return std::nullopt;
    } else {
        return std::string(*cand);
    }
}

inline std::optional<std::string>
did_you_mean(std::string_view given, std::initializer_list<std::string_view> strings) noexcept {
    return did_you_mean(given, ranges::views::all(strings));
}

template <typename Range>
e_did_you_mean calc_e_did_you_mean(std::string_view given, Range&& strings) noexcept {
    return {did_you_mean(given, strings)};
}

inline e_did_you_mean calc_e_did_you_mean(std::string_view                        given,
                                          std::initializer_list<std::string_view> il) noexcept {
    return calc_e_did_you_mean(given, ranges::views::all(il));
}

}  // namespace dds