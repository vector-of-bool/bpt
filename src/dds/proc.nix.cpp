#ifndef _WIN32
#include "./proc.hpp"

#include <dds/util/signal.hpp>

#include <spdlog/spdlog.h>

#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <deque>
#include <iostream>
#include <system_error>

using namespace dds;

namespace {

void check_rc(bool b, std::string_view s) {
    if (!b) {
        throw std::system_error(std::error_code(errno, std::system_category()), std::string(s));
    }
}

::pid_t
spawn_child(const std::vector<std::string>& command, int stdout_pipe, int close_me) noexcept {
    auto child_pid = ::fork();
    if (child_pid != 0) {
        return child_pid;
    }
    // We are child
    ::close(close_me);
    auto rc = dup2(stdout_pipe, STDOUT_FILENO);
    check_rc(rc != -1, "Failed to dup2 stdout");
    rc = dup2(stdout_pipe, STDERR_FILENO);
    check_rc(rc != -1, "Failed to dup2 stderr");

    std::vector<const char*> strings;
    strings.reserve(command.size() + 1);
    for (auto& s : command) {
        strings.push_back(s.data());
    }
    strings.push_back(nullptr);
    ::execvp(strings[0], (char* const*)strings.data());

    if (errno == ENOENT) {
        std::cerr
            << fmt::format("[dds child executor] The requested executable ({}) could not be found.",
                           strings[0]);
        std::exit(-1);
    }

    std::cerr << "[dds child executor] execvp returned! This is a fatal error: "
              << std::system_category().message(errno) << '\n';

    std::terminate();
}

}  // namespace

proc_result dds::run_proc(const std::vector<std::string>& command) {
    spdlog::debug("Spawning subprocess: {}", quote_command(command));
    int  stdio_pipe[2] = {};
    auto rc            = ::pipe(stdio_pipe);
    check_rc(rc == 0, "Create stdio pipe for subprocess");

    int read_pipe  = stdio_pipe[0];
    int write_pipe = stdio_pipe[1];

    auto child = spawn_child(command, write_pipe, read_pipe);

    ::close(write_pipe);

    pollfd stdio_fd;
    stdio_fd.fd     = read_pipe;
    stdio_fd.events = POLLIN;

    proc_result res;

    while (true) {
        rc = ::poll(&stdio_fd, 1, -1);
        if (rc && errno == EINTR) {
            errno = 0;
            continue;
        }
        check_rc(rc > 0, "Failed in poll()");
        std::string buffer;
        buffer.resize(1024);
        auto nread = ::read(stdio_fd.fd, buffer.data(), buffer.size());
        if (nread == 0) {
            break;
        }
        check_rc(nread > 0, "Failed in read()");
        res.output.append(buffer.begin(), buffer.begin() + nread);
    }

    int status = 0;
    rc         = ::waitpid(child, &status, 0);
    check_rc(rc >= 0, "Failed in waitpid()");

    if (WIFEXITED(status)) {
        res.retc = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        res.signal = WTERMSIG(status);
    }

    cancellation_point();
    return res;
}

#endif  // _WIN32