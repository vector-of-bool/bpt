#include "./version.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>

using namespace semver;

using siter = std::string_view::iterator;

namespace {

version parse(const char* ptr, std::size_t size) {
    version ret;
    // const auto str_begin = ptr;
    const auto str_end = ptr + size;

    std::from_chars_result fc_res;
    auto did_error = [&](int elem) { return fc_res.ec == std::errc::invalid_argument || elem < 0; };

    // Parse major
    fc_res = std::from_chars(ptr, str_end, ret.major);
    if (did_error(ret.major) || fc_res.ptr == str_end || *fc_res.ptr != '.') {
        throw invalid_version(0);
    }

    // Parse minor
    ptr    = fc_res.ptr + 1;
    fc_res = std::from_chars(ptr, str_end, ret.minor);
    if (did_error(ret.minor) || fc_res.ptr == str_end || *fc_res.ptr != '.') {
        throw invalid_version(ptr - str_end);
    }

    // Parse patch
    ptr    = fc_res.ptr + 1;
    fc_res = std::from_chars(ptr, str_end, ret.patch);
    if (did_error(ret.patch)) {
        throw invalid_version(ptr - str_end);
    }

    if (fc_res.ptr != str_end) {
        assert(false && "More complex version numbers are not ready yet!");
        throw invalid_version(-42);
    }

    return ret;
}

}  // namespace

version version::parse(std::string_view s) { return ::parse(s.data(), s.size()); }

std::string version::to_string() const noexcept {
    std::array<char, 256> buffer;
    const auto            buf_begin = buffer.data();
    auto                  buf_ptr   = buf_begin;
    const auto            buf_end   = buf_ptr + buffer.size();

    auto conv_one = [&](auto n) {
        auto tc_res = std::to_chars(buf_ptr, buf_end, n);
        if (tc_res.ec == std::errc::value_too_large || tc_res.ptr == buf_end) {
            assert(false && "Our buffers weren't big enough! This is a bug!");
            std::terminate();
        }
        buf_ptr = tc_res.ptr;
    };

    conv_one(major);
    *buf_ptr++ = '.';
    conv_one(minor);
    *buf_ptr++ = '.';
    conv_one(patch);

    return std::string(buf_begin, (buf_ptr - buf_begin));
}