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
    case errc::no_such_catalog_package:
        return "no-such-catalog-package.html";
    case errc::git_url_ref_mutual_req:
        return "git-url-ref-mutual-req.html";
    case errc::test_failure:
        return "test-failure.html";
    case errc::compile_failure:
        return "compile-failure.html";
    case errc::archive_failure:
        return "archive-failure.html";
    case errc::link_failure:
        return "link-failure.html";
    case errc::catalog_too_new:
        return "catalog-too-new.html";
    case errc::corrupted_catalog_db:
        return "corrupted-catalog-db.html";
    case errc::invalid_catalog_json:
        return "invalid-catalog-json.html";
    case errc::no_catalog_remote_info:
        return "no-catalog-remote-info.html";
    case errc::git_clone_failure:
        return "git-clone-failure.html";
    case errc::sdist_ident_mismatch:
        return "sdist-ident-mismatch.html";
    case errc::corrupted_build_db:
        return "corrupted-build-db.html";
    case errc::invalid_version_range_string:
        return "invalid-version-string.html#range";
    case errc::invalid_version_string:
        return "invalid-version-string.html";
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
    case errc::catalog_too_new:
        return R"(
The catalog database file contains a schema that will automatically be upgraded
by dds when it is opened/modified. It appears that the given catalog database
has had a migration beyond a version that we support. Has the catalog been
modified by a newer version of dds?
)";
    case errc::corrupted_catalog_db:
        return R"(
The catalog database schema doesn't match what dds expects. This indicates that
the database file has been modified in a way that dds cannot automatically fix
and handle.
)";
    case errc::invalid_catalog_json:
        return R"(
The catalog JSON that was provided does not match the format that was expected.
Check the JSON schema and try your submission again.
)";
    case errc::no_catalog_remote_info:
        return R"(
The catalog entry requires information regarding the remote acquisition method.
Refer to the documentation for details.
)";
    case errc::git_clone_failure:
        return R"(
dds tried to clone a repository using Git, but the clone operation failed.
There are a variety of possible causes. It is best to check the output from
Git in diagnosing this failure.
)";
    case errc::sdist_ident_mismatch:
        return R"(
We tried to automatically generate a source distribution from a package, but
the name and/or version of the package that was generated does not match what
we expected of it.
)";
    case errc::corrupted_build_db:
        return R"(
The local build database file is corrupted. The file is stored in the build
directory as `.dds.db', and is safe to delete to clear the bad data. This is
not a likely error, and if you receive this message frequently, please file a
bug report.
)";
    case errc::invalid_version_range_string:
        return R"(
Parsing of a version range string failed. Refer to the documentation for more
information.
)";
    case errc::invalid_version_string:
        return R"(
`dds` expects all version numbers to conform to the Semantic Versioning
specification. Refer to the documentation and https://semver.org/ for more
information.
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
    case errc::compile_failure:
        return "Source compilation failed.";
    case errc::archive_failure:
        return "Creating a static library archive failed";
    case errc::link_failure:
        return "Linking a runtime binary (executable/shared library/DLL) failed";
    case errc::catalog_too_new:
        return "The catalog appears to be from a newer version of dds.";
    case errc::corrupted_catalog_db:
        return "The catalog database appears to be corrupted or invalid";
    case errc::invalid_catalog_json:
        return "The given catalog JSON data is not valid";
    case errc::no_catalog_remote_info:
        return "The catalog JSON is missing remote acquisition information for one or more\n"
               "packages";
    case errc::git_clone_failure:
        return "A git-clone operation failed.";
    case errc::sdist_ident_mismatch:
        return "The package version of a generated source distribution did not match the version\n"
               "that was expected of it";
    case errc::corrupted_build_db:
        return "The build database file is corrupted";
    case errc::invalid_version_range_string:
        return "Attempted to parse an invalid version range string. <- (Seeing this text is a "
               "`dds` bug. Please report it.)";
    case errc::invalid_version_string:
        return "Attempted to parse an invalid version string. <- (Seeing this text is a `dds` bug. "
               "Please report it.)";
    case errc::none:
        break;
    }
    assert(false && "Unexpected execution path during error message creation. This is a DDS bug");
    std::terminate();
}
