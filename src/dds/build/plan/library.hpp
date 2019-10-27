#pragma once

#include <dds/build/plan/archive.hpp>
#include <dds/build/plan/exe.hpp>
#include <dds/library.hpp>
#include <dds/usage_reqs.hpp>
#include <dds/util/fs.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace dds {

class library_plan {
    std::string                        _name;
    fs::path                           _source_root;
    std::optional<create_archive_plan> _create_archive;
    std::vector<link_executable_plan>  _link_exes;

public:
    library_plan(std::string_view                   name,
                 path_ref                           source_root,
                 std::optional<create_archive_plan> ar,
                 std::vector<link_executable_plan>  exes)
        : _name(name)
        , _source_root(source_root)
        , _create_archive(std::move(ar))
        , _link_exes(std::move(exes)) {}

    path_ref source_root() const noexcept { return _source_root; }
    auto&    name() const noexcept { return _name; }
    auto&    create_archive() const noexcept { return _create_archive; }
    auto&    executables() const noexcept { return _link_exes; }

    static library_plan
    create(const library&, const library_build_params&, const usage_requirement_map&);
};

}  // namespace dds