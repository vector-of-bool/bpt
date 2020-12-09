#ifndef _WIN32
#include "./proc.hpp"

#include <dds/util/fs.hpp>
#include <dds/util/log.hpp>
#include <dds/util/signal.hpp>

#include <poll.h>
#include <signal.h>
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

::pid_t spawn_child(const proc_options& opts, int stdout_pipe, int close_me) noexcept {
    // We must allocate BEFORE fork(), since the CRT might stumble with malloc()-related locks that
    // are held during the fork().
    std::vector<const char*> strings;
    strings.reserve(opts.command.size() + 1);
    for (auto& s : opts.command) {
        strings.push_back(s.data());
    }
    strings.push_back(nullptr);

    std::string workdir = opts.cwd.value_or(fs::current_path()).string();

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
    rc = ::chdir(workdir.data());
    check_rc(rc != -1, "Failed to chdir() for subprocess");

    ::execvp(strings[0], (char* const*)strings.data());

    if (errno == ENOENT) {
        std::cerr
            << fmt::format("[dds child executor] The requested executable ({}) could not be found.",
                           strings[0]);
        std::exit(-1);
    }

    std::cerr << "[dds child executor] execvp returned! This is a fatal error: "
              << std::system_category().message(errno) << '\n';

    std::exit(-1);
}

}  // namespace

proc_result dds::run_proc(const proc_options& opts) {
    dds_log(debug, "Spawning subprocess: {}", quote_command(opts.command));
    int  stdio_pipe[2] = {};
    auto rc            = ::pipe(stdio_pipe);
    check_rc(rc == 0, "Create stdio pipe for subprocess");

    int read_pipe  = stdio_pipe[0];
    int write_pipe = stdio_pipe[1];

    auto child = spawn_child(opts, write_pipe, read_pipe);

    ::close(write_pipe);

    pollfd stdio_fd;
    stdio_fd.fd     = read_pipe;
    stdio_fd.events = POLLIN;

    proc_result res;

    using namespace std::chrono_literals;

    /// Quirk: We _could_ just use opts.timeout.value_or, but it seems like something
    /// is weird in GCC 9's data flow analysis and it will warn that `timeout` is
    /// used uninitialized when its value is passed to poll() ??
    auto timeout = -1ms;
    if (opts.timeout) {
        timeout = *opts.timeout;
    }
    while (true) {
        rc = ::poll(&stdio_fd, 1, static_cast<int>(timeout.count()));
        if (rc && errno == EINTR) {
            errno = 0;
            continue;
        }
        if (rc == 0) {
            // Timeout!
            ::kill(child, SIGINT);
            timeout       = -1ms;
            res.timed_out = true;
            dds_log(debug, "Subprocess [{}] timed out", quote_command(opts.command));
            continue;
        }
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