#pragma once

#include <dds/source.hpp>

#include <optional>
#include <string>

namespace dds {

struct library_ident {
    std::string namespace_;
    std::string name;
};

class library {
    fs::path    _base_dir;
    std::string _name;
    source_list _sources;
    fs::path    _pub_inc_dir;
    fs::path    _priv_inc_dir;

    library(path_ref dir, std::string_view name, source_list&& src)
        : _base_dir(dir)
        , _name(name)
        , _sources(std::move(src)) {}

public:
    static library from_directory(path_ref, std::string_view name);
    static library from_directory(path_ref path) {
        return from_directory(path, path.filename().string());
    }

    path_ref base_dir() const noexcept { return _base_dir; }
    path_ref public_include_dir() const noexcept { return _pub_inc_dir; }
    path_ref private_include_dir() const noexcept { return _priv_inc_dir; }

    const source_list& sources() const noexcept { return _sources; }
};

}  // namespace dds