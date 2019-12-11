#pragma once

#include <dds/util/fs.hpp>

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
            if (fs::exists(path, ec)) {
                fs::remove_all(path, ec);
            }
        }
    };

    std::shared_ptr<impl> _ptr;

    temporary_dir(std::shared_ptr<impl> p)
        : _ptr(p) {}

public:
    static temporary_dir create();

    path_ref path() const noexcept { return _ptr->path; }
};

}  // namespace dds