#ifdef _WIN32

#include "./flock.hpp"

#include <dds/util/signal.hpp>

#include <fmt/core.h>
#include <wil/resource.h>

#include <cassert>

using namespace dds;

namespace {

enum fail_mode {
    blocking,
    nonblocking,
};

enum lock_mode {
    shared,
    exclusive,
};

struct lock_data {
    wil::unique_handle file;

    bool do_lock(fail_mode fm, lock_mode lm, path_ref p) {
        auto       lockflags = lm == exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0;
        auto       failflags = fm == nonblocking ? LOCKFILE_FAIL_IMMEDIATELY : 0;
        auto       flags     = lockflags | failflags;
        auto       hndl      = file.get();
        OVERLAPPED ovr       = {};
        const auto okay      = ::LockFileEx(hndl, flags, 0, 0, 0, &ovr);
        if (!okay) {
            cancellation_point();
            if (::GetLastError() == ERROR_IO_PENDING) {
                return false;
            }
            throw std::system_error(std::error_code(errno, std::system_category()),
                                    fmt::format("Failed to modify file lock [{}]", p.string()));
        }
        return true;
    }

    void unlock(path_ref p) {
        OVERLAPPED ovr  = {};
        const auto okay = ::UnlockFileEx(file.get(), 0, 0, 0, &ovr);
        if (!okay) {
            cancellation_point();
            throw std::system_error(std::error_code(::GetLastError(), std::system_category()),
                                    fmt::format("Failed to unlock file [{}]", p.string()));
        }
    }
};

}  // namespace

#define THIS_DATA static_cast<lock_data*>(_lock_data)

shared_file_mutex::shared_file_mutex(path_ref filepath)
    : _path{filepath} {
    auto               h = ::CreateFileW(_path.native().c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr,
                           CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    wil::unique_handle file{h};

    if (!file) {
        throw std::system_error(std::error_code(::GetLastError(), std::system_category()),
                                fmt::format("Failed to open file for locking [{}]",
                                            _path.string()));
    }

    _lock_data = new lock_data(lock_data{std::move(file)});
}

#define MY_LOCK_DATA (*static_cast<lock_data*>(_lock_data))

shared_file_mutex::~shared_file_mutex() {
    assert(_lock_data);
    delete &MY_LOCK_DATA;
    _lock_data = nullptr;
}

bool shared_file_mutex::try_lock() noexcept {
    // Attempt to take an exclusive lock
    return MY_LOCK_DATA.do_lock(nonblocking, exclusive, _path);
}

bool shared_file_mutex::try_lock_shared() noexcept {
    // Take a non-exclusive lock
    return MY_LOCK_DATA.do_lock(nonblocking, shared, _path);
}

void shared_file_mutex::lock() {
    // Blocking exclusive lock
    MY_LOCK_DATA.do_lock(blocking, exclusive, _path);
}

void shared_file_mutex::lock_shared() {
    // Blocking shared lock
    MY_LOCK_DATA.do_lock(blocking, shared, _path);
}

void shared_file_mutex::unlock() {
    // Unlock
    MY_LOCK_DATA.unlock(_path);
}

void shared_file_mutex::unlock_shared() { unlock(); }

#endif