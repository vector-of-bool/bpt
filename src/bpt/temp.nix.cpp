#include "./temp.hpp"

#ifndef _WIN32

#ifdef __APPLE__
#include <unistd.h>
#endif

using namespace bpt;

temporary_dir temporary_dir::create_in(path_ref base) {
    fs::create_directories(base);

    auto file = (base / "bpt-tmp-XXXXXX").string();
    const char* tempdir_path = ::mkdtemp(file.data());
    if (tempdir_path == nullptr) {
        throw std::system_error(std::error_code(errno, std::system_category()),
                                "Failed to create a temporary directory");
    }
    auto path = fs::path(tempdir_path);
    return std::make_shared<impl>(std::move(path));
}

#endif
