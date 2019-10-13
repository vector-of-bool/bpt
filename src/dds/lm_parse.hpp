#ifndef DDS_LM_PARSE_HPP_INCLUDED
#define DDS_LM_PARSE_HPP_INCLUDED

#include <cassert>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace dds {

class lm_pair {
    std::string _key;
    std::string _value;

public:
    lm_pair(std::string_view k, std::string_view v)
        : _key(k)
        , _value(v) {}

    auto& key() const noexcept { return _key; }
    auto& value() const noexcept { return _value; }
};

struct kv_pair_iterator {
    using vec_type  = std::vector<lm_pair>;
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

    inline kv_pair_iterator(base_iter, base_iter, std::string_view k);

    kv_pair_iterator& operator++() & noexcept {
        assert(_iter != _end);
        ++_iter;
        while (_iter != _end && _iter->key() != _key) {
            ++_iter;
        }
        return *this;
    }

    const lm_pair* operator->() const noexcept {
        assert(_iter != _end);
        return _iter.operator->();
    }

    const lm_pair& operator*() const noexcept {
        assert(_iter != _end);
        return *_iter;
    }

    inline bool operator!=(const kv_pair_iterator& o) const noexcept { return _iter != o._iter; }

    auto begin() const noexcept { return *this; }
    auto end() const noexcept { return kv_pair_iterator(_end, _end, _key); }

    explicit operator bool() const noexcept { return *this != end(); }
};

class lm_kv_pairs {
    std::vector<lm_pair> _kvs;

public:
    explicit lm_kv_pairs(std::vector<lm_pair> kvs)
        : _kvs(kvs) {}

    auto& items() const noexcept { return _kvs; }

    const lm_pair* find(const std::string_view& key) const noexcept {
        for (auto&& item : items()) {
            if (item.key() == key) {
                return &item;
            }
        }
        return nullptr;
    }

    kv_pair_iterator iter(std::string_view key) const noexcept {
        auto       iter = items().begin();
        const auto end  = items().end();
        while (iter != end && iter->key() != key) {
            ++iter;
        }
        return kv_pair_iterator{iter, end, key};
    }

    std::vector<lm_pair> all_of(std::string_view key) const noexcept {
        auto iter = this->iter(key);
        return std::vector<lm_pair>(iter, iter.end());
    }

    auto size() const noexcept { return _kvs.size(); }
};

inline kv_pair_iterator::kv_pair_iterator(base_iter it, base_iter end, std::string_view k)
    : _iter{it}
    , _end{end}
    , _key{k} {}

lm_kv_pairs lm_parse_string(std::string_view);
lm_kv_pairs lm_parse_file(std::filesystem::path);
void        lm_write_pairs(std::filesystem::path, const std::vector<lm_pair>&);

inline void lm_write_pairs(const std::filesystem::path& fpath, const lm_kv_pairs& pairs) {
    lm_write_pairs(fpath, pairs.items());
}

}  // namespace dds

#endif  // DDS_LM_PARSE_HPP_INCLUDED