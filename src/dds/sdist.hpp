#pragma once

#include <dds/util/fs.hpp>

namespace dds {

struct sdist_params {
    fs::path project_dir;
    fs::path dest_path;
    bool     force         = false;
    bool     include_apps  = false;
    bool     include_tests = false;
};

class sdist {
    std::string _name;
    std::string _version;
    std::string _hash;
    fs::path    _sdist_dir;

public:
    sdist(std::string_view name, std::string_view version, std::string_view hash, path_ref path)
        : _name(name)
        , _version(version)
        , _hash(hash)
        , _sdist_dir(path) {}

    static sdist from_directory(path_ref p);

    std::string_view name() const noexcept { return _name; }
    std::string_view version() const noexcept { return _version; }
    std::string_view hash() const noexcept { return _hash; }
    path_ref         path() const noexcept { return _sdist_dir; }

    std::string ident() const noexcept { return _name + "." + _version + "." + _hash; }
};

sdist create_sdist(const sdist_params&);
sdist create_sdist_in_dir(path_ref, const sdist_params&);

}  // namespace dds