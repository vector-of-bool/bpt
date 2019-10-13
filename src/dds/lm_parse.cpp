#include "./lm_parse.hpp"

#include <dds/util.hpp>

#include <fstream>

namespace fs = std::filesystem;

using namespace std::literals;

using namespace dds;

namespace {

std::string_view sview(std::string_view::const_iterator beg, std::string_view::const_iterator end) {
    return std::string_view{beg, static_cast<std::size_t>(std::distance(beg, end))};
}

std::string_view trim(std::string_view s) {
    auto iter = s.begin();
    auto end  = s.end();
    while (iter != end && std::isspace(*iter)) {
        ++iter;
    }
    auto riter = s.rbegin();
    auto rend  = s.rend();
    while (riter != rend && std::isspace(*riter)) {
        ++riter;
    }
    auto new_end = riter.base();
    return sview(iter, new_end);
}

void parse_line(std::vector<lm_pair>& pairs, const std::string_view whole_line) {
    const auto line = trim(whole_line);
    if (line.empty() || line[0] == '#') {
        return;
    }

    const auto begin = line.begin();
    auto       iter  = begin;
    const auto end   = line.end();

    while (true) {
        if (iter == end) {
            throw std::runtime_error("Invalid line in config file: '"s + std::string(whole_line)
                                     + "'");
        }
        if (*iter == ':') {
            if (++iter == end) {
                // Empty value
                break;
            } else if (*iter == ' ') {
                // Found the key
                break;
            } else {
                // Just a regular character. Keep going...
            }
        }
        ++iter;
    }

    // `iter` now points to the space between the key and value
    auto key   = sview(begin, iter - 1);  // -1 to trim the colon in the key
    auto value = sview(iter, end);
    key        = trim(key);
    value      = trim(value);
    pairs.emplace_back(key, value);
}

}  // namespace

dds::lm_kv_pairs dds::lm_parse_string(std::string_view s) {
    std::vector<lm_pair> pairs;

    auto line_begin = s.begin();
    auto iter       = line_begin;
    auto end        = s.end();

    while (iter != end) {
        if (*iter == '\n') {
            parse_line(pairs, sview(line_begin, iter));
            line_begin = ++iter;
            continue;
        }
        ++iter;
    }
    if (line_begin != end) {
        parse_line(pairs, sview(line_begin, end));
    }
    return lm_kv_pairs(std::move(pairs));
}

dds::lm_kv_pairs dds::lm_parse_file(fs::path fpath) { return lm_parse_string(slurp_file(fpath)); }

void dds::lm_write_pairs(fs::path fpath, const std::vector<lm_pair>& pairs) {
    auto fstream = open(fpath, std::ios::out | std::ios::binary);
    for (auto& pair : pairs) {
        fstream << pair.key() << ": " << pair.value() << '\n';
    }
}