#pragma once

#include <dds/util/fs.hpp>

namespace dds {

class shared_file_mutex {
    fs::path _path;
    void*    _lock_data = nullptr;

public:
    shared_file_mutex(path_ref p);

    shared_file_mutex(const shared_file_mutex&) = delete;

    ~shared_file_mutex();

    path_ref path() const noexcept { return _path; }

    bool try_lock() noexcept;
    bool try_lock_shared() noexcept;
    void lock();
    void lock_shared();
    void unlock();
    void unlock_shared();
};

}  // namespace dds
