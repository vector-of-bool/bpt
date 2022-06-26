#pragma once

#include <filesystem>

namespace bpt {

class shared_file_mutex {
    std::filesystem::path _path;
    void*                 _lock_data = nullptr;

public:
    shared_file_mutex(const std::filesystem::path& p);

    shared_file_mutex(const shared_file_mutex&) = delete;

    ~shared_file_mutex();

    const std::filesystem::path& path() const noexcept { return _path; }

    bool try_lock() noexcept;
    bool try_lock_shared() noexcept;
    void lock();
    void lock_shared();
    void unlock();
    void unlock_shared();
};

}  // namespace bpt
