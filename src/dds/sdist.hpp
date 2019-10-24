#pragma once

#include <dds/package_manifest.hpp>
#include <dds/util/fs.hpp>

#include <browns/md5.hpp>
#include <browns/output.hpp>

namespace dds {

struct sdist_params {
    fs::path project_dir;
    fs::path dest_path;
    bool     force         = false;
    bool     include_apps  = false;
    bool     include_tests = false;
};

struct sdist {
    package_manifest         manifest;
    browns::md5::digest_type md5;
    fs::path                 path;

    sdist(package_manifest man, browns::md5::digest_type hash, path_ref path)
        : manifest(std::move(man))
        , md5(hash)
        , path(path) {}

    static sdist from_directory(path_ref p);

    std::string md5_string() const noexcept { return browns::format_digest(md5); }

    std::string ident() const noexcept {
        return manifest.name + "." + manifest.version.to_string() + "." + md5_string();
    }
};

sdist create_sdist(const sdist_params&);
sdist create_sdist_in_dir(path_ref, const sdist_params&);

}  // namespace dds