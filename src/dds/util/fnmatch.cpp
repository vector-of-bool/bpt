#include "./fnmatch.hpp"

#include <cassert>
#include <stdexcept>
#include <string_view>
#include <vector>

using charptr = const char*;

namespace dds::detail::fnmatch {

namespace {

class base_pattern_elem {
public:
    virtual bool match(charptr first, charptr last) const noexcept = 0;
    virtual ~base_pattern_elem()                                   = default;

    std::unique_ptr<base_pattern_elem> next;
};

class rt_star : public base_pattern_elem {
    bool match(charptr first, charptr last) const noexcept {
        while (first != last) {
            auto did_match = next->match(first, last);
            if (did_match) {
                return true;
            }
            ++first;
        }
        // We're at the end. Try once more
        return next->match(first, last);
    }
};

class rt_any_char : public base_pattern_elem {
    bool match(charptr first, charptr last) const noexcept {
        if (first == last) {
            return false;
        }
        return next->match(first + 1, last);
    }
};

class rt_oneof : public base_pattern_elem {
    std::string _chars;
    bool        match(charptr first, charptr last) const noexcept {
        if (first == last) {
            return false;
        }
        auto idx = _chars.find(*first);
        if (idx == _chars.npos) {
            return false;
        }
        return next->match(first + 1, last);
    }

public:
    explicit rt_oneof(std::string chars)
        : _chars(chars) {}
};

class rt_lit : public base_pattern_elem {
    std::string _lit;
    bool        match(charptr first, charptr last) const noexcept {
        auto remaining = static_cast<std::size_t>(std::distance(first, last));
        if (remaining < _lit.size()) {
            return false;
        }
        auto eq = std::equal(first, first + _lit.size(), _lit.begin());
        if (!eq) {
            return false;
        }
        return next->match(first + _lit.size(), last);
    }

public:
    explicit rt_lit(std::string lit)
        : _lit(lit) {}
};

class rt_end : public base_pattern_elem {
    bool match(charptr first, charptr last) const noexcept { return first == last; }
};

}  // namespace

class pattern_impl {
    std::unique_ptr<base_pattern_elem>  _head;
    std::unique_ptr<base_pattern_elem>* _next_to_compile = &_head;

    template <typename T, typename... Args>
    void _add_elem(Args&&... args) {
        *_next_to_compile = std::make_unique<T>(std::forward<Args>(args)...);
        _next_to_compile  = &(*_next_to_compile)->next;
    }

    charptr _compile_oneof(charptr cur, charptr last) {
        std::string chars;
        while (cur != last) {
            auto c = *cur;
            if (c == ']') {
                // We've reached the end of the group
                _add_elem<rt_oneof>(chars);
                return cur + 1;
            }
            if (c == '\\') {
                ++cur;
                if (cur == last) {
                    throw std::runtime_error("Untermated [group] in pattern");
                }
                chars.push_back(*cur);
            } else {
                chars.push_back(c);
            }
            ++cur;
        }
        throw std::runtime_error("Unterminated [group] in pattern");
    }

    charptr _compile_lit(charptr cur, charptr last) {
        std::string lit;
        while (cur != last) {
            auto c = *cur;
            if (c == '*' || c == '[' || c == '?') {
                break;
            }
            if (c == '\\') {
                ++cur;
                if (cur == last) {
                    throw std::runtime_error("Invalid \\ at end of pattern");
                }
                // Push back whatever character follows
                lit.push_back(*cur);
                ++cur;
                continue;
            } else {
                lit.push_back(c);
            }
            ++cur;
        }
        _add_elem<rt_lit>(lit);
        return cur;
    }

    void _compile_next(charptr first, charptr last) {
        if (first == last) {
            return;
        }
        auto c = *first;
        if (c == '*') {
            _add_elem<rt_star>();
            _compile_next(first + 1, last);
        } else if (c == '[') {
            first = _compile_oneof(first + 1, last);
            _compile_next(first, last);
        } else if (c == '?') {
            _add_elem<rt_any_char>();
            _compile_next(first + 1, last);
        } else {
            // Literal string
            first = _compile_lit(first, last);
            _compile_next(first, last);
        }
    }

public:
    pattern_impl(std::string_view str) {
        _compile_next(str.data(), str.data() + str.size());
        // Set the tail of the list to be an rt_end to detect end-of-string
        _add_elem<rt_end>();
    }

    bool match(charptr first, charptr last) const noexcept {
        assert(_head);
        return _head->match(first, last);
    }
};

}  // namespace dds::detail::fnmatch

dds::fnmatch::pattern dds::fnmatch::compile(std::string_view str) {
    return pattern{std::make_shared<detail::fnmatch::pattern_impl>(str)};
}

bool dds::fnmatch::pattern::_match(charptr first, charptr last) const noexcept {
    assert(_impl);
    return _impl->match(first, last);
}
