#pragma once

#include <dds/build/compile.hpp>
#include <dds/build/source_dir.hpp>
#include <dds/library_manifest.hpp>
#include <dds/source.hpp>

#include <optional>
#include <string>

namespace dds {

struct library_ident {
    std::string namespace_;
    std::string name;
};

class library {
    fs::path         _path;
    std::string      _name;
    source_list      _sources;
    library_manifest _man;

    library(path_ref dir, std::string_view name, source_list&& src, library_manifest&& man)
        : _path(dir)
        , _name(name)
        , _sources(std::move(src))
        , _man(std::move(man)) {}

public:
    static library from_directory(path_ref, std::string_view name);
    static library from_directory(path_ref path) {
        return from_directory(path, path.filename().string());
    }

    auto& name() const noexcept { return _name; }

    auto& manifest() const noexcept { return _man; }

    source_directory src_dir() const noexcept { return source_directory{path() / "src"}; }
    source_directory include_dir() const noexcept { return source_directory{path() / "include"}; }

    path_ref path() const noexcept { return _path; }
    fs::path public_include_dir() const noexcept;
    fs::path private_include_dir() const noexcept;

    const source_list&        all_sources() const noexcept { return _sources; }
    shared_compile_file_rules base_compile_rules() const noexcept;
};

struct library_build_params {
    fs::path                  out_subdir;
    bool                      build_tests = false;
    bool                      build_apps  = false;
    std::vector<fs::path>     rt_link_libraries;
    shared_compile_file_rules compile_rules;
};

}  // namespace dds