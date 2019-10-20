#ifndef _WIN32
#include "./temp.hpp"

using namespace dds;

temporary_dir temporary_dir::create() {
    auto base = fs::temp_directory_path();
    auto file = (base / "dds-tmp-XXXXXX").string();

    const char* tempdir_path = ::mktemp(file.data());

    if (tempdir_path == nullptr) {
        throw std::system_error(std::error_code(errno, std::system_category()),
                                "Failed to create a temporary directory");
    }
    auto path = fs::path(tempdir_path);
    return std::make_shared<impl>(std::move(path));
}
#endif