#pragma once

#include <dds/build/plan/archive.hpp>
#include <dds/build/plan/exe.hpp>
#include <dds/library/library.hpp>
#include <dds/usage_reqs.hpp>
#include <dds/util/fs.hpp>

#include <optional>
#include <string>
#include <vector>

namespace dds {

struct library_build_params {
    fs::path out_subdir;
    bool     build_tests     = false;
    bool     build_apps      = false;
    bool     enable_warnings = false;

    // Extras for compiling tests:
    std::vector<fs::path> test_include_dirs;
    std::vector<fs::path> test_link_files;
};

class library_plan {
    std::string                        _name;
    fs::path                           _source_root;
    std::optional<create_archive_plan> _create_archive;
    std::vector<link_executable_plan>  _link_exes;
    std::vector<lm::usage>             _uses;
    std::vector<lm::usage>             _links;

public:
    library_plan(std::string_view                   name,
                 path_ref                           source_root,
                 std::optional<create_archive_plan> ar,
                 std::vector<link_executable_plan>  exes,
                 std::vector<lm::usage>             uses,
                 std::vector<lm::usage>             links)
        : _name(name)
        , _source_root(source_root)
        , _create_archive(std::move(ar))
        , _link_exes(std::move(exes))
        , _uses(std::move(uses))
        , _links(std::move(links)) {}

    path_ref source_root() const noexcept { return _source_root; }
    auto&    name() const noexcept { return _name; }
    auto&    create_archive() const noexcept { return _create_archive; }
    auto&    executables() const noexcept { return _link_exes; }
    auto&    uses() const noexcept { return _uses; }
    auto&    links() const noexcept { return _links; }

    static library_plan
    create(const library&, const library_build_params&, const usage_requirement_map&);
};

}  // namespace dds