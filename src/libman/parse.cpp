#include "./parse.hpp"

#include <bpt/util/fs/io.hpp>
#include <libman/util.hpp>

#include <fmt/core.h>

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

lm::pair_list lm::parse_file(fs::path fpath) { return parse_string(bpt::read_file(fpath)); }

void lm::write_pairs(fs::path fpath, const std::vector<pair>& pairs) {
    auto fstream = bpt::open_file(fpath, std::ios::out | std::ios::binary);
    for (auto& pair : pairs) {
        fstream << pair.key << ": " << pair.value << '\n';
    }
}

nested_kvlist nested_kvlist::parse(const std::string_view line_) {
    const auto line     = trim_view(line_);
    const auto semi_pos = line.find(';');
    const auto primary  = trim_view(line.substr(0, semi_pos));
    auto       tail     = semi_pos == line.npos ? ""sv : trim_view(line.substr(semi_pos + 1));

    std::vector<pair> pairs;
    while (!tail.empty()) {
        const auto space_pos = tail.find(' ');
        const auto item      = tail.substr(0, space_pos);

        const auto eq_pos = item.find('=');
        if (eq_pos == item.npos) {
            pairs.emplace_back(item, ""sv);
        } else {
            const auto key   = item.substr(0, eq_pos);
            const auto value = item.substr(eq_pos + 1);
            pairs.emplace_back(key, value);
        }

        if (space_pos == tail.npos) {
            break;
        }
        tail = trim_view(tail.substr(space_pos + 1));
    }

    return nested_kvlist{std::string(primary), pair_list{std::move(pairs)}};
}