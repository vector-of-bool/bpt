#pragma once

#include <json5/data.hpp>

#include <tuple>
#include <variant>

namespace dds {

namespace json_read {

struct reject_t {
    std::string message;
};

struct accept_t {};
struct pass_t {};

using result_var = std::variant<reject_t, accept_t, pass_t>;

inline namespace ops {

struct reject {
    std::string_view message;

    result_var operator()(const json5::data&) const noexcept {
        return reject_t{std::string(message)};
    }
};

template <typename... Handlers>
struct then {
    std::tuple<Handlers...> _hs;

    explicit then(Handlers... hs)
        : _hs(std::move(hs)...) {}

    result_var _handle(const json5::data&) noexcept { return pass_t{}; }

    template <typename Head, typename... Tail>
    result_var _handle(const json5::data& dat, Head&& h, Tail&&... tail) {
        result_var res = h(dat);
        if (!std::holds_alternative<pass_t>(res)) {
            return res;
        }
        return _handle(dat, tail...);
    }

    result_var operator()(const json5::data& dat) {
        return std::apply([&](auto&&... hs) { return _handle(dat, hs...); }, _hs);
    }
};

template <typename... KeyHandlers>
struct object {
    std::tuple<KeyHandlers...> _keys;

    explicit object(KeyHandlers... ks)
        : _keys(ks...) {}

    result_var _handle(std::string_view, const json5::data&) { return pass_t{}; }

    template <typename Head, typename... Tail>
    result_var _handle(std::string_view key, const json5::data& dat, Head cur, Tail... ts) {
        result_var current = cur(key, dat);
        if (std::holds_alternative<pass_t>(current)) {
            return _handle(key, dat, ts...);
        }
        return current;
    }

    result_var operator()(const json5::data& dat) {
        if (!dat.is_object()) {
            return pass_t{};
        }

        for (const auto& [key, val] : dat.as_object()) {
            result_var res
                = std::apply([&](auto... ks) { return _handle(key, val, ks...); }, _keys);
            if (std::holds_alternative<accept_t>(res)) {
                continue;
            }
            if (std::holds_alternative<reject_t>(res)) {
                return res;
            }
        }

        return accept_t{};
    }
};

template <typename Handler>
struct array_each {
    Handler _hs;

    result_var operator()(const json5::data& arr) {
        if (!arr.is_array()) {
            return pass_t{};
        }
        for (const auto& elem : arr.as_array()) {
            result_var res = _hs(elem);
            if (std::holds_alternative<reject_t>(res)) {
                return res;
            }
        }
        return accept_t{};
    }
};

template <typename Handler>
array_each(Handler) -> array_each<Handler>;

template <typename Handler>
struct key {
    std::string_view _key;
    Handler          _handle;

    key(std::string_view k, Handler h)
        : _key(k)
        , _handle(h) {}

    result_var operator()(std::string_view key, const json5::data& dat) {
        if (key == _key) {
            return _handle(dat);
        }
        return pass_t{};
    }
};

inline struct reject_key_t {
    result_var operator()(std::string_view key, const json5::data&) const noexcept {
        return reject_t{"The key `" + std::string(key) + "` is invalid"};
    }
} reject_key;

struct ignore_key {
    std::string_view key;

    result_var operator()(std::string_view key, const json5::data&) const noexcept {
        if (key == this->key) {
            return accept_t{};
        }
        return pass_t{};
    }
};

template <typename T, typename Handler>
struct accept_type {
    Handler _handle;

    result_var operator()(const json5::data& d) {
        if (!d.is<T>()) {
            return pass_t{};
        }
        return _handle(d);
    }
};

template <typename T, typename Handler>
auto accept(Handler h) {
    return accept_type<T, Handler>{h};
}

template <typename H>
auto if_string(H h) {
    return accept<std::string>(h);
}

template <typename H>
auto require_string(H h, std::string_view msg) {
    return then(if_string(h), reject{msg});
}

template <typename T>
struct put_into {
    T& _dest;

    result_var operator()(const json5::data& d) {
        _dest = d.as<T>();
        return accept_t{};
    }
};

template <typename T>
put_into(T) -> put_into<T>;

}  // namespace ops

template <typename Handler>
auto decompose(const json5::data& dat, Handler&& h) {
    result_var res = h(dat);
    if (std::holds_alternative<reject_t>(res)) {
        throw std::runtime_error(std::get<reject_t>(res).message);
    }
}

}  // namespace json_read

}  // namespace dds