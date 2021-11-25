#pragma once

#include <dds/util/fs/path.hpp>

#include <memory>

namespace dds {

class temporary_dir {
    struct impl {
        fs::path path;
        explicit impl(path_ref p)
            : path(p) {}

        impl(const impl&) = delete;

        ~impl() {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    };

    std::shared_ptr<impl> _ptr;

    temporary_dir(std::shared_ptr<impl> p)
        : _ptr(p) {}

public:
    static temporary_dir create_in(path_ref parent);
    static temporary_dir create() { return create_in(fs::temp_directory_path()); }

    path_ref path() const noexcept { return _ptr->path; }
};

}  // namespace dds