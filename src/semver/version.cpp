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
    const auto str_begin = ptr;
    const auto str_end   = ptr + size;
    auto       get_str   = [=] { return std::string(str_begin, size); };
    auto       cur_off   = [&] { return ptr - str_begin; };

    std::from_chars_result fc_res;
    auto did_error = [&](int elem) { return fc_res.ec == std::errc::invalid_argument || elem < 0; };

    // Parse major
    fc_res = std::from_chars(ptr, str_end, ret.major);
    ptr    = fc_res.ptr;
    if (did_error(ret.major) || fc_res.ptr == str_end || *fc_res.ptr != '.') {
        throw invalid_version(get_str(), cur_off());
    }

    // Parse minor
    ptr    = fc_res.ptr + 1;
    fc_res = std::from_chars(ptr, str_end, ret.minor);
    ptr    = fc_res.ptr;
    if (did_error(ret.minor) || fc_res.ptr == str_end || *fc_res.ptr != '.') {
        throw invalid_version(get_str(), cur_off());
    }

    // Parse patch
    ptr    = fc_res.ptr + 1;
    fc_res = std::from_chars(ptr, str_end, ret.patch);
    ptr    = fc_res.ptr;
    if (did_error(ret.patch)) {
        throw invalid_version(get_str(), cur_off());
    }

    auto remaining = std::string_view(ptr, str_end - ptr);
    if (!remaining.empty() && remaining[0] == '-') {
        auto plus_pos       = remaining.find('+');
        auto prerelease_str = remaining.substr(1, plus_pos - 1);
        ret.prerelease      = prerelease::parse(prerelease_str);
        remaining           = remaining.substr(prerelease_str.size() + 1);
    }

    if (!remaining.empty() && remaining[0] == '+') {
        auto bmeta_str     = remaining.substr(1);
        ret.build_metadata = build_metadata::parse(bmeta_str);
        remaining          = remaining.substr(bmeta_str.size() + 1);
    }

    if (!remaining.empty()) {
        throw invalid_version(get_str(), remaining.data() - str_begin);
    }

    return ret;
}  // namespace

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

    auto main_ver = std::string(buf_begin, (buf_ptr - buf_begin));

    auto format_ids = [](auto& ids) {
        std::string acc;
        auto        it   = ids.cbegin();
        auto        stop = ids.cend();
        while (it != stop) {
            acc += it->string();
            ++it;
            if (it != stop) {
                acc += ".";
            }
        }
        return acc;
    };

    if (!prerelease.empty()) {
        main_ver += "-" + format_ids(prerelease.idents());
    }
    if (!build_metadata.empty()) {
        main_ver += "+" + format_ids(build_metadata.idents());
    }
    return main_ver;
}

order semver::compare(const version& lhs, const version& rhs) noexcept {
    auto lhs_tup = std::tie(lhs.major, lhs.minor, lhs.patch);
    auto rhs_tup = std::tie(rhs.major, rhs.minor, rhs.patch);
    if (lhs_tup < rhs_tup) {
        return order::less;
    } else if (lhs_tup > rhs_tup) {
        return order::greater;
    } else {
        if (!lhs.is_prerelease() && rhs.is_prerelease()) {
            // No prerelease is greater than any prerelease
            return order::greater;
        } else if (lhs.is_prerelease() && !rhs.is_prerelease()) {
            // A prerelease version is lesser than any non-prerelease version
            return order::less;
        } else {
            // Compare the prerelease tags
            return compare(lhs.prerelease, rhs.prerelease);
        }
    }
}