#include "./errors.hpp"

#include <cassert>
#include <stdexcept>

using namespace dds;

namespace {

std::string error_url_prefix = "http://localhost:3000/err/";

std::string error_url_suffix(dds::errc ec) noexcept {
    switch (ec) {
    case errc::invalid_builtin_toolchain:
        return "invalid-builtin-toolchain.html";
    case errc::no_such_catalog_package:
        return "no-such-catalog-package.html";
    case errc::git_url_ref_mutual_req:
        return "git-url-ref-mutual-req.html";
    case errc::none:
        break;
    }
    assert(false && "Unreachable code path generating error explanation URL");
    std::terminate();
}

}  // namespace

std::string dds::error_reference_of(dds::errc ec) noexcept {
    return error_url_prefix + error_url_suffix(ec);
}

std::string_view dds::explanation_of(dds::errc ec) noexcept {
    switch (ec) {
    case errc::invalid_builtin_toolchain:
        return R"(
If you start your toolchain name (The `-t` or `--toolchain` argument)
with a leading colon, dds will interpret it as a reference to a built-in
toolchain. (Toolchain file paths cannot begin with a leading colon).

These toolchain names are encoded into the dds executable and cannot be
modified.
)";
    case errc::no_such_catalog_package:
        return R"(
The installation of a package was requested, but the given package ID was not
able to be found in the package catalog. Check the spelling and version number.
)";
    case errc::git_url_ref_mutual_req:
        return R"(
Creating a Git-based catalog entry requires both a URL to clone from and a Git
reference (tag, branch, commit) to clone.
)";
    case errc::none:
        break;
    }
    assert(false && "Unexpected execution path during error explanation. This is a DDS bug");
    std::terminate();
}

std::string_view dds::default_error_string(dds::errc ec) noexcept {
    switch (ec) {
    case errc::invalid_builtin_toolchain:
        return "The built-in toolchain name is invalid";
    case errc::no_such_catalog_package:
        return "The catalog has no entry for the given package ID";
    case errc::git_url_ref_mutual_req:
        return "Git requires both a URL and a ref to clone";
    case errc::none:
        break;
    }
    assert(false && "Unexpected execution path during error message creation. This is a DDS bug");
    std::terminate();
}