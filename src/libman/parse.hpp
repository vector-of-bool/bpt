#pragma once

#include <libman/parse_fwd.hpp>

#include <cassert>
#include <filesystem>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace lm {

class pair {
public:
    std::string key;
    std::string value;

    pair(std::string_view k, std::string_view v)
        : key(k)
        , value(v) {}
};

class pair_iterator {
    using vec_type  = std::vector<pair>;
    using base_iter = vec_type::const_iterator;
    base_iter   _iter;
    base_iter   _end;
    std::string _key;

public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = vec_type::difference_type;
    using value_type        = vec_type::value_type;
    using pointer           = vec_type::pointer;
    using reference         = vec_type::reference;

    inline pair_iterator(base_iter, base_iter, std::string_view k);

    pair_iterator& operator++() & noexcept {
        assert(_iter != _end);
        ++_iter;
        while (_iter != _end && _iter->key != _key) {
            ++_iter;
        }
        return *this;
    }

    const pair* operator->() const noexcept {
        assert(_iter != _end);
        return _iter.operator->();
    }

    const pair& operator*() const noexcept {
        assert(_iter != _end);
        return *_iter;
    }

    inline bool operator!=(const pair_iterator& o) const noexcept { return _iter != o._iter; }
    inline bool operator==(const pair_iterator& o) const noexcept { return _iter == o._iter; }

    auto begin() const noexcept { return *this; }
    auto end() const noexcept { return pair_iterator(_end, _end, _key); }

    explicit operator bool() const noexcept { return *this != end(); }
};

class pair_list {
    std::vector<pair> _kvs;

public:
    explicit pair_list(std::vector<pair> kvs)
        : _kvs(kvs) {}

    auto& items() const noexcept { return _kvs; }

    const pair* find(const std::string_view& key) const noexcept {
        for (auto&& item : items()) {
            if (item.key == key) {
                return &item;
            }
        }
        return nullptr;
    }

    pair_iterator iter(std::string_view key) const noexcept {
        auto       iter = items().begin();
        const auto end  = items().end();
        while (iter != end && iter->key != key) {
            ++iter;
        }
        return pair_iterator{iter, end, key};
    }

    std::vector<pair> all_of(std::string_view key) const noexcept {
        auto iter = this->iter(key);
        return std::vector<pair>(iter, iter.end());
    }

    auto size() const noexcept { return _kvs.size(); }
};

inline pair_iterator::pair_iterator(base_iter it, base_iter end, std::string_view k)
    : _iter{it}
    , _end{end}
    , _key{k} {}

pair_list parse_string(std::string_view);
pair_list parse_file(std::filesystem::path);
void      write_pairs(std::filesystem::path, const std::vector<pair>&);

inline void write_pairs(const std::filesystem::path& fpath, const pair_list& pairs) {
    write_pairs(fpath, pairs.items());
}

struct nested_kvlist {
    std::string primary;
    pair_list   pairs;

    static nested_kvlist parse(std::string_view s);
};

struct unchanged {
    template <typename Item>
    auto operator()(Item&& item) const {
        return item;
    }
};

template <typename What>
class read_required {
    std::string_view _key;
    What&            _ref;
    bool             _did_read = false;

public:
    read_required(std::string_view key, What& ref)
        : _key(key)
        , _ref(ref) {}

    int operator()(std::string_view context, std::string_view key, std::string_view value) {
        if (key != _key) {
            return 0;
        }
        if (_did_read) {
            throw std::runtime_error(std::string(context) + ": Duplicated key '" + std::string(key)
                                     + "' is not allowed");
        }
        _did_read = true;
        _ref      = What(value);
        return 1;
    }

    void validate(std::string_view context) const {
        if (!_did_read) {
            throw std::runtime_error(std::string(context) + ": Missing required key '"
                                     + std::string(_key) + "'");
        }
    }
};

template <typename T, typename Transform = unchanged>
class read_opt {
    std::string_view _key;
    T&               _ref;
    bool             _did_read = false;
    Transform        _tr;

public:
    read_opt(std::string_view key, T& ref, Transform tr = unchanged())
        : _key(key)
        , _ref(ref)
        , _tr(std::move(tr)) {}

    int operator()(std::string_view context, std::string_view key, std::string_view value) {
        if (key != _key) {
            return 0;
        }
        if (_did_read) {
            throw std::runtime_error(std::string(context) + ": Duplicated key '" + std::string(key)
                                     + "' is not allowed.");
        }
        _ref = T(_tr(value));
        return 1;
    }
};

template <typename T>
class read_bool {
    std::string_view _key;
    T&               _ref;
    bool             _did_read = false;

public:
    read_bool(std::string_view key, T& ref)
        : _key(key)
        , _ref(ref) {}

    bool operator()(std::string_view context, std::string_view key, std::string_view value) {
        if (key != _key) {
            return false;
        }
        if (_did_read) {
            throw std::runtime_error(std::string(context) + ": Duplicate key '" + std::string(key)
                                     + "' is not allowed.");
        }
        if (value == "true" || value == "True") {
            _ref = true;
        } else if (value == "false" || value == "False") {
            _ref = false;
        } else {
            throw std::runtime_error(std::string(context) + ": Invalid value '" + std::string(value)
                                     + "' for key '" + std::string(key)
                                     + ".' Expected `true` or `false`.");
        }
        _did_read = true;
        return true;
    }
};

struct read_empty_set_true {
    std::string_view _key;
    bool&            _bool;
    bool             _seen = false;

    bool operator()(std::string_view context, std::string_view key, std::string_view value) {
        if (key != _key) {
            return false;
        }
        if (value != "") {
            throw std::runtime_error(std::string(context) + ": Key '" + std::string(key)
                                     + "' does not expected a value (Got '" + std::string(value)
                                     + "').");
        }
        if (_seen) {
            throw std::runtime_error(std::string(context) + ": Duplicated key '" + std::string(key)
                                     + "'");
        }
        _bool = true;
        _seen = true;
        return true;
    }
};

class read_check_eq {
    std::string_view _key;
    std::string_view _expect;

public:
    read_check_eq(std::string_view key, std::string_view value)
        : _key(key)
        , _expect(value) {}

    int operator()(std::string_view context, std::string_view key, std::string_view value) const {
        if (key != _key) {
            return 0;
        }
        if (value != _expect) {
            throw std::runtime_error(std::string(context) + ": Expected key '" + std::string(key)
                                     + "' to have value '" + std::string(_expect) + "' (Got '"
                                     + std::string(value) + "')");
        }
        return 1;
    }
};

template <typename Container, typename Transform = unchanged>
class read_accumulate {
    std::string_view _key;
    Container&       _items;
    Transform        _tr;

public:
    read_accumulate(std::string_view key, Container& c, Transform tr)
        : _key(key)
        , _items(c)
        , _tr(std::move(tr)) {}

    read_accumulate(std::string_view key, Container& c)
        : _key(key)
        , _items(c)
        , _tr(unchanged()) {}

    int operator()(std::string_view, std::string_view key, std::string_view value) {
        if (key == _key) {
            _items.emplace_back(_tr(value));
            return 1;
        }
        return 0;
    }
};

class ignore_x_keys {
public:
    bool operator()(std::string_view, std::string_view key, std::string_view) const {
        return key.find("X-") == 0;
    }
};

class reject_unknown {
public:
    int operator()(std::string_view context, std::string_view key, std::string_view) const {
        throw std::runtime_error(std::string(context) + ": Unknown key '" + std::string(key) + "'");
    }
};

template <typename T>
auto validate_reader(T&& t, std::string_view context, int) -> decltype(t.validate(context)) {
    t.validate(context);
}

template <typename T>
void validate_reader(T&&, std::string_view, ...) {}

template <typename... Items>
auto read(std::string_view context [[maybe_unused]], const pair_list& pairs, Items... is) {
    std::vector<pair> bad_pairs;
    for (auto [key, value] : pairs.items()) {
        auto did_read = (is(context, key, value) || ...);
        if (did_read) {
            bad_pairs.emplace_back(key, value);
        }
    }
    (validate_reader(is, context, 0), ...);
    return bad_pairs;
}

}  // namespace lm
