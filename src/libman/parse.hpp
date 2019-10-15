#pragma once

#include <cassert>
#include <filesystem>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace lm {

class pair {
    std::string _key;
    std::string _value;

public:
    pair(std::string_view k, std::string_view v)
        : _key(k)
        , _value(v) {}

    auto& key() const noexcept { return _key; }
    auto& value() const noexcept { return _value; }

    template <std::size_t I>
    std::string_view get() const {
        if constexpr (I == 0) {
            return key();
        } else if constexpr (I == 1) {
            return value();
        }
    }
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
        while (_iter != _end && _iter->key() != _key) {
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
            if (item.key() == key) {
                return &item;
            }
        }
        return nullptr;
    }

    pair_iterator iter(std::string_view key) const noexcept {
        auto       iter = items().begin();
        const auto end  = items().end();
        while (iter != end && iter->key() != key) {
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

template <typename What>
class read_required {
    std::string_view _key;
    What&            _ref;
    bool             _did_read = false;

public:
    read_required(std::string_view key, What& ref)
        : _key(key)
        , _ref(ref) {}

    int read_one(std::string_view context, std::string_view key, std::string_view value) {
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

template <typename T>
class read_opt {
    std::string_view  _key;
    std::optional<T>& _ref;

public:
    read_opt(std::string_view key, std::optional<T>& ref)
        : _key(key)
        , _ref(ref) {}

    int read_one(std::string_view context, std::string_view key, std::string_view value) {
        if (key != _key) {
            return 0;
        }
        if (_ref.has_value()) {
            throw std::runtime_error(std::string(context) + ": Duplicated key '" + std::string(key)
                                     + "' is not allowed.");
        }
        _ref.emplace(value);
        return 1;
    }

    void validate(std::string_view) {}
};

class read_check_eq {
    std::string_view _key;
    std::string_view _expect;

public:
    read_check_eq(std::string_view key, std::string_view value)
        : _key(key)
        , _expect(value) {}

    int read_one(std::string_view context, std::string_view key, std::string_view value) const {
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

    void validate(std::string_view) {}
};

template <typename Container>
class read_accumulate {
    std::string_view _key;
    Container&       _items;

public:
    read_accumulate(std::string_view key, Container& c)
        : _key(key)
        , _items(c) {}

    int read_one(std::string_view, std::string_view key, std::string_view value) const {
        if (key == _key) {
            _items.emplace_back(value);
            return 1;
        }
        return 0;
    }

    void validate(std::string_view) {}
};

template <typename... Items>
auto read(std::string_view context [[maybe_unused]], const pair_list& pairs, Items... is) {
    std::vector<pair> bad_pairs;
    for (auto [key, value] : pairs.items()) {
        auto nread = (is.read_one(context, key, value) + ... + 0);
        if (nread == 0) {
            bad_pairs.emplace_back(key, value);
        }
    }
    (is.validate(context), ...);
    return bad_pairs;
}

}  // namespace lm

namespace std {

template <>
struct tuple_size<lm::pair> : std::integral_constant<int, 2> {};

template <std::size_t N>
struct tuple_element<N, lm::pair> {
    using type = std::string_view;
};

}  // namespace std