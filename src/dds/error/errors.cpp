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
    case errc::test_failure:
        return "test-failure.html";
    case errc::archive_failure:
        return "archive-failure.html";
    case errc::link_failure:
        return "link-failure.html";
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
    case errc::test_failure:
        return R"(
One or more of the project's tests failed. The failing tests are listed above,
along with their exit code and output.
)";
    case errc::archive_failure:
        return R"(
Creating a static library archive failed, which prevents the associated library
from being used as this archive is the input to the linker for downstream
build targets.

It is unlikely that regular user action can cause static library archiving to
fail. Refer to the output of the archiving tool.
)";
    case errc::link_failure:
        return R"(
Linking a runtime binary file failed. There are a variety of possible causes
for this error. Refer to the documentation for more information.
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
    case errc::test_failure:
        return "One or more tests failed";
    case errc::archive_failure:
        return "Creating a static library archive failed";
    case errc::link_failure:
        return "Linking a runtime binary (executable/shared library/DLL) failed";
    case errc::none:
        break;
    }
    assert(false && "Unexpected execution path during error message creation. This is a DDS bug");
    std::terminate();
}
