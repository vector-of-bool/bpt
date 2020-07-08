#pragma once

#include <cstdlib>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>

namespace dds {

namespace fnmatch {

template <typename... Elems>
struct ct_pattern;

class pattern;

}  // namespace fnmatch

namespace detail::fnmatch {

template <typename... Sub>
struct seq {};

struct star {};

struct any_one {};

template <typename Char>
constexpr std::size_t length(const Char* str) {
    std::size_t ret = 0;
    while (*str != Char(0)) {
        ++str;
        ++ret;
    }
    return ret;
}

template <auto... Chars>
struct oneof {};

template <auto... Chars>
struct not_oneof {};

template <auto Char>
struct just {};

template <typename>
struct is_just : std::false_type {};
template <auto C>
struct is_just<just<C>> : std::true_type {};

template <typename Matcher, auto NewCur>
struct oneof_ret {
    using type                       = Matcher;
    constexpr static auto end_offset = NewCur;
};

template <auto... Chars, auto End>
constexpr auto negate(oneof_ret<oneof<Chars...>, End>) {
    return oneof_ret<not_oneof<Chars...>, End>();
}

template <auto Cur, auto Len, auto... Chars, typename String>
constexpr auto compile_oneof_chars(String s) {
    constexpr auto str      = s();
    constexpr auto cur_char = str[Cur];
    static_assert(Cur != Len, "Unterminated '[' group in pattern");
    static_assert(Cur + 1 != Len || cur_char != '\\', "Escape \\ at end of pattern");
    if constexpr (cur_char == ']') {
        return oneof_ret<oneof<Chars...>, Cur + 1>();
    } else if constexpr (cur_char == '\\') {
        constexpr auto next_char = str[Cur + 1];
        return compile_oneof_chars<Cur + 2, Len, Chars..., next_char>(s);
    } else {
        return compile_oneof_chars<Cur + 1, Len, Chars..., cur_char>(s);
    }
}

template <auto Cur, auto Len, typename String>
constexpr auto compile_oneof(String s) {
    constexpr auto str         = s();
    constexpr bool negated     = str[Cur] == '!';
    constexpr auto oneof_start = Cur + (negated ? 1 : 0);
    auto           oneof       = compile_oneof_chars<oneof_start, Len>(s);
    if constexpr (negated) {
        return negate(oneof);
    } else {
        return oneof;
    }
}

template <auto Cur, auto Len, typename... Matchers, typename String>
constexpr auto compile_next(String s) {
    constexpr auto str      = s();
    constexpr auto cur_char = str[Cur];
    if constexpr (Cur == Len) {
        return dds::fnmatch::ct_pattern<Matchers...>();
    } else if constexpr (cur_char == '*') {
        return compile_next<Cur + 1, Len, Matchers..., star>(s);
    } else if constexpr (cur_char == '?') {
        return compile_next<Cur + 1, Len, Matchers..., any_one>(s);
    } else if constexpr (cur_char == '[') {
        constexpr auto oneof_ret = compile_oneof<Cur + 1, Len>(s);
        return compile_next<oneof_ret.end_offset,
                            Len,
                            Matchers...,
                            typename decltype(oneof_ret)::type>(s);
    } else if constexpr (cur_char == '\\') {
        // Escape sequence
        static_assert(Cur + 1 != Len, "Escape \\ at end of pattern.");
        constexpr auto next_char = str[Cur + 1];
        return compile_next<Cur + 2, Len, Matchers..., just<next_char>>(s);
    } else {
        return compile_next<Cur + 1, Len, Matchers..., just<cur_char>>(s);
    }
}

template <typename Iter1, typename Iter2>
constexpr bool equal(Iter1 a_first, Iter1 a_last, Iter2 b_first) {
    while (a_first != a_last) {
        if (*a_first != *b_first) {
            return false;
        }
        ++a_first;
        ++b_first;
    }
    return true;
}

}  // namespace detail::fnmatch

namespace fnmatch {

template <typename... Elems>
struct ct_pattern {
private:
    /// VVVVVVVVVVVVVVVVVVV Optimized Cases VVVVVVVVVVVVVVVVVVVVVVV

    /**
     * Common case of a star '*' followed by literals to the end of the pattern
     */
    template <typename Iter, auto C, auto... Chars>
    static constexpr bool match_1(Iter       cur,
                                  const Iter last,
                                  detail::fnmatch::star,
                                  detail::fnmatch::just<C> c1,
                                  detail::fnmatch::just<Chars>... t) {
        // We know the length of tail required, so we can just skip ahead without
        // a loop
        auto cur_len = std::distance(cur, last);
        if (cur_len < sizeof...(Chars) + 1) {
            // Not enough remaining to match
            return false;
        }
        // Skip ahead and match the rest
        auto to_skip = cur_len - (sizeof...(Chars) + 1);
        return match_1(std::next(cur, to_skip), last, c1, t...);
    }

    /**
     * Common case of a sequence of literals at the tail.
     */
    template <typename Iter, auto... Chars>
    static constexpr bool match_1(Iter cur, const Iter last, detail::fnmatch::just<Chars>...) {
        constexpr auto LitLength = sizeof...(Chars);
        auto           remaining = std::distance(cur, last);
        if (remaining != LitLength) {
            return false;
        }
        // Put our characters into an array for a quick comparison
        std::decay_t<decltype(*cur)> chars[LitLength] = {Chars...};
        return detail::fnmatch::equal(chars, chars + LitLength, cur);
    }

    /// VVVVVVVVVVVVVVVVVVVV General cases VVVVVVVVVVVVVVVVVVVVVVVV
    template <typename Iter, typename... Tail>
    static constexpr bool match_1(Iter cur, const Iter last, detail::fnmatch::star, Tail... t) {
        while (cur != last) {
            auto did_match = match_1(cur, last, t...);
            if (did_match) {
                return true;
            }
            ++cur;
        }
        // We've advanced to the end of the string, but we might still have a match...
        return match_1(cur, last, t...);
    }

    template <typename Iter, auto... Chars, typename... Tail>
    static constexpr bool
    match_1(Iter cur, const Iter last, detail::fnmatch::not_oneof<Chars...>, Tail... t) {
        if (cur == last) {
            return false;
        }
        if (((*cur == Chars) || ...)) {
            return false;
        }
        return match_1(std::next(cur), last, t...);
    }

    template <typename Iter, auto... Chars, typename... Tail>
    static constexpr bool
    match_1(Iter cur, const Iter last, detail::fnmatch::oneof<Chars...>, Tail... t) {
        if (cur == last) {
            return false;
        }
        if (((*cur == Chars) || ...)) {
            return match_1(std::next(cur), last, t...);
        } else {
            // current char is not in pattern
            return false;
        }
    }

    template <typename Iter,
              auto C,
              typename... Tail,
              // Only enable this overload if the tail is not entirely just<> items
              // (we have an optimization for that case)
              typename = std::enable_if_t<!(detail::fnmatch::is_just<Tail>() && ...)>>
    static constexpr bool match_1(Iter cur, const Iter last, detail::fnmatch::just<C>, Tail... t) {
        if (cur == last) {
            // We've reached the end, but we have more things to match
            return false;
        }
        if (*cur != C) {
            // Wrong char
            return false;
        } else {
            // Good char, keep going
            return match_1(std::next(cur), last, t...);
        }
    }

    template <typename Iter, typename... Tail>
    static constexpr bool match_1(Iter cur, const Iter last, detail::fnmatch::any_one, Tail... t) {
        if (cur == last) {
            return false;
        }
        return match_1(std::next(cur), last, t...);
    }

    template <typename Iter>
    static constexpr bool match_1(Iter cur, Iter last) {
        return cur == last;
    }

public:
    static constexpr bool match(const char* fname) {
        return match_1(fname, fname + detail::fnmatch::length(fname), Elems()...);
    }
};

template <typename StringGenerator, typename = decltype(std::declval<StringGenerator&>()())>
constexpr auto compile(StringGenerator&& s) {
    constexpr auto pattern = s();
    constexpr auto len     = detail::fnmatch::length(pattern);
    return decltype(detail::fnmatch::compile_next<0, len>(s))();
}

pattern compile(std::string_view str);

}  // namespace fnmatch

namespace detail::fnmatch {

class pattern_impl;

}  // namespace detail::fnmatch

namespace fnmatch {

class pattern {
    std::shared_ptr<const detail::fnmatch::pattern_impl> _impl;

    bool _match(const char* begin, const char* end) const noexcept;

public:
    constexpr static std::size_t noalloc_size = 256;

    pattern(std::shared_ptr<const detail::fnmatch::pattern_impl> ptr)
        : _impl(ptr) {}
    ~pattern()              = default;
    pattern(const pattern&) = default;
    pattern(pattern&&)      = default;
    pattern& operator=(const pattern&) = default;
    pattern& operator=(pattern&&) = default;

    template <typename Iter>
    bool match(Iter first, Iter last) const {
        auto dist = static_cast<std::size_t>(std::distance(first, last));
        if (dist < noalloc_size) {
            char buffer[noalloc_size];
            auto buf_end = std::copy(first, last, buffer);
            return _match(buffer, buf_end);
        } else {
            // Allocates
            std::string str(first, last);
            return _match(str.data(), str.data() + str.size());
        }
    }

    bool match(const char* str) const {
        return match(str, str + dds::detail::fnmatch::length(str));
    }

    template <typename Seq>
    bool match(const Seq& seq) const {
        using std::begin;
        using std::end;
        return match(begin(seq), end(seq));
    }

    std::optional<std::string> literal_spelling() const noexcept;
};

}  // namespace fnmatch

}  // namespace dds
