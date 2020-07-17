#ifndef _WIN32

#include "./flock.hpp"

#include <dds/util/signal.hpp>

#include <fmt/core.h>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

using namespace dds;

namespace {

struct lock_data {
    int fd;

    bool do_lock(int fcntl_kind, short int lock_kind, path_ref p) {
        struct ::flock lk = {};
        lk.l_type         = lock_kind;
        lk.l_len          = 0;
        lk.l_whence       = SEEK_SET;
        lk.l_start        = 0;
        auto rc           = ::fcntl(fd, fcntl_kind, &lk);
        if (rc == -1) {
            cancellation_point();
            if (errno == EAGAIN || errno == EACCES) {
                return false;
            }
            throw std::system_error(std::error_code(errno, std::system_category()),
                                    fmt::format("Failed to modify file lock [{}]", p.string()));
        }
        return true;
    }
};

}  // namespace

#define THIS_DATA static_cast<lock_data*>(_lock_data)

shared_file_mutex::shared_file_mutex(path_ref filepath)
    : _path{filepath} {
    int fd = ::open(_path.string().c_str(), O_CREAT | O_CLOEXEC | O_RDWR, 0b110'100'100);
    if (fd < 0) {
        throw std::system_error(std::error_code(errno, std::system_category()),
                                fmt::format("Failed to open file for locking [{}]",
                                            _path.string()));
    }
    _lock_data = new lock_data(lock_data{fd});
}

#define MY_LOCK_DATA (*static_cast<lock_data*>(_lock_data))

shared_file_mutex::~shared_file_mutex() {
    assert(_lock_data);
    ::close(MY_LOCK_DATA.fd);
    delete &MY_LOCK_DATA;
    _lock_data = nullptr;
}

bool shared_file_mutex::try_lock() noexcept {
    // Attempt to take an exclusive lock
    return MY_LOCK_DATA.do_lock(F_SETLK, F_WRLCK, _path);
}

bool shared_file_mutex::try_lock_shared() noexcept {
    // Take a non-exclusive lock
    return MY_LOCK_DATA.do_lock(F_SETLK, F_RDLCK, _path);
}

void shared_file_mutex::lock() {
    // Blocking exclusive lock
    MY_LOCK_DATA.do_lock(F_SETLKW, F_WRLCK, _path);
}

void shared_file_mutex::lock_shared() {
    // Blocking shared lock
    MY_LOCK_DATA.do_lock(F_SETLKW, F_RDLCK, _path);
}

void shared_file_mutex::unlock() {
    // Unlock
    MY_LOCK_DATA.do_lock(F_SETLK, F_UNLCK, _path);
}

void shared_file_mutex::unlock_shared() { unlock(); }

#endif