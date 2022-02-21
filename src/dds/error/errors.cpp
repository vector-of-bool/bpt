#include "./errors.hpp"

#include <cassert>
#include <stdexcept>

using namespace dds;

namespace {

std::string error_url_prefix = "https://vector-of-bool.github.io/docs/dds/err/";

std::string error_url_suffix(dds::errc ec) noexcept {
    switch (ec) {
    case errc::invalid_builtin_toolchain:
        return "invalid-builtin-toolchain.html";
    case errc::no_default_toolchain:
        return "no-default-toolchain.html";
    case errc::test_failure:
        return "test-failure.html";
    case errc::compile_failure:
        return "compile-failure.html";
    case errc::archive_failure:
        return "archive-failure.html";
    case errc::link_failure:
        return "link-failure.html";
    case errc::invalid_remote_url:
        return "invalid-remote-url.html";
    case errc::invalid_pkg_filesystem:
        return "invalid-pkg-filesystem.html";
    case errc::sdist_exists:
        return "sdist-exists.html";
    case errc::cyclic_usage:
        return "cyclic-usage.html";
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
    case errc::no_default_toolchain:
        return R"(
`dds` requires a toolchain to be specified in order to execute the build. `dds`
will not perform a "best-guess" at a default toolchain. You may either pass the
name of a built-in toolchain, or write a "default toolchain" file to one of the
supported filepaths. Refer to the documentation for more information.
)";
    case errc::test_failure:
        return R"(
One or more of the project's tests failed. The failing tests are listed above,
along with their exit code and output.
)";
    case errc::compile_failure:
        return R"(
Source compilation failed. Refer to the compiler output.
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
    case errc::invalid_remote_url:
        return R"(The given package/remote URL is invalid)";
    case errc::invalid_pkg_filesystem:
        return R"(
`dds` prescribes a specific filesystem structure that must be obeyed by
libraries and packages. Refer to the documentation for an explanation and
reference on these prescriptions.
)";
    case errc::sdist_exists:
        return R"(
By default, `dds` will not overwrite source distributions that already exist
(either in the repository or a filesystem path). Such an action could
potentially destroy important data.
)";
    case errc::cyclic_usage:
        return R"(
A cyclic dependency was detected among the libraries' `uses` fields. The cycle
must be removed. If no cycle is apparent, check that the `uses` field for the
library does not refer to the library itself.
)";
    case errc::none:
        break;
    }
    assert(false && "Unexpected execution path during error explanation. This is a DDS bug");
    std::terminate();
}

#define BUG_STRING_SUFFIX " <- (Seeing this text is a `dds` bug. Please report it.)"

std::string_view dds::default_error_string(dds::errc ec) noexcept {
    switch (ec) {
    case errc::invalid_builtin_toolchain:
        return "The built-in toolchain name is invalid";
    case errc::no_default_toolchain:
        return "Unable to find a default toolchain to use for the build";
    case errc::test_failure:
        return "One or more tests failed";
    case errc::compile_failure:
        return "Source compilation failed.";
    case errc::archive_failure:
        return "Creating a static library archive failed";
    case errc::link_failure:
        return "Linking a runtime binary (executable/shared library/DLL) failed";
    case errc::invalid_remote_url:
        return "The given package/remote URL is not valid";
    case errc::invalid_pkg_filesystem:
        return "The filesystem structure of the package/library is invalid." BUG_STRING_SUFFIX;
    case errc::sdist_exists:
        return "The source ditsribution already exists at the destination " BUG_STRING_SUFFIX;
    case errc::cyclic_usage:
        return "A cyclic dependency was detected among the libraries' `uses` fields.";
    case errc::none:
        break;
    }
    assert(false && "Unexpected execution path during error message creation. This is a DDS bug");
    std::terminate();
}
