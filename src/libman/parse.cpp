#include "./parse.hpp"

#include <libman/util.hpp>

#include <spdlog/fmt/fmt.h>

#include <cctype>
#include <fstream>

namespace fs = std::filesystem;

using namespace std::literals;

using namespace lm;

namespace {

void parse_line(std::vector<pair>& pairs, const std::string_view whole_line) {
    const auto line = trim_view(whole_line);
    if (line.empty() || line[0] == '#') {
        return;
    }

    const auto begin = line.begin();
    auto       iter  = begin;
    const auto end   = line.end();

    while (true) {
        if (iter == end) {
            throw std::runtime_error(fmt::format("Invalid line in config file: '{}'", whole_line));
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
    key        = trim_view(key);
    value      = trim_view(value);
    pairs.emplace_back(key, value);
}

}  // namespace

pair_list lm::parse_string(std::string_view s) {
    std::vector<pair> pairs;

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
    return pair_list(std::move(pairs));
}

lm::pair_list lm::parse_file(fs::path fpath) { return parse_string(dds::slurp_file(fpath)); }

void lm::write_pairs(fs::path fpath, const std::vector<pair>& pairs) {
    auto fstream = dds::open(fpath, std::ios::out | std::ios::binary);
    for (auto& pair : pairs) {
        fstream << pair.key() << ": " << pair.value() << '\n';
    }
}