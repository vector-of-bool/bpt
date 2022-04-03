#include "./usage.hpp"

#include <bpt/error/result.hpp>

#include <boost/leaf/error.hpp>

#include <ostream>

using namespace lm;

dds::result<usage> lm::split_usage_string(std::string_view str) {
    auto sl_pos = str.find('/');
    if (sl_pos == str.npos) {
        return boost::leaf::new_error(e_invalid_usage_string{std::string(str)});
    }
    auto ns   = str.substr(0, sl_pos);
    auto name = str.substr(sl_pos + 1);
    return usage{std::string(ns), std::string(name)};
}

void usage::write_to(std::ostream& out) const noexcept { out << namespace_ << "/" << name; }
