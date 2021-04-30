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

::pid_t spawn_child(const proc_options& opts,
                    int                 stdout_pipe,
                    int                 close_me,
                    int                 close_me2,
                    int                 stdin_pipe) noexcept {
    // We must allocate BEFORE fork(), since the CRT might stumble with malloc()-related locks that
    // are held during the fork().
    std::vector<const char*> strings;
    strings.reserve(opts.command.size() + 1);
    for (auto& s : opts.command) {
        strings.push_back(s.data());
    }
    strings.push_back(nullptr);

    std::string workdir = opts.cwd.value_or(fs::current_path()).string();
    auto        not_found_err
        = fmt::format("[dds child executor] The requested executable [{}] could not be found.",
                      strings[0]);

    auto child_pid = ::fork();
    if (child_pid != 0) {
        return child_pid;
    }
    // We are child
    ::close(close_me);
    ::close(close_me2);
    auto rc = dup2(stdout_pipe, STDOUT_FILENO);
    check_rc(rc != -1, "Failed to dup2 stdout");
    rc = dup2(stdout_pipe, STDERR_FILENO);
    check_rc(rc != -1, "Failed to dup2 stderr");
    rc = dup2(stdin_pipe, STDIN_FILENO);
    check_rc(rc != -1, "Failed to dup2 stdin");
    rc = ::chdir(workdir.data());
    check_rc(rc != -1, "Failed to chdir() for subprocess");

    ::execvp(strings[0], (char* const*)strings.data());

    if (errno == ENOENT) {
        std::fputs(not_found_err.c_str(), stderr);
        std::_Exit(-1);
    }

    std::fputs("[dds child executor] execvp returned! This is a fatal error: ", stderr);
    std::fputs(std::strerror(errno), stderr);
    std::fputs("\n", stderr);
    std::_Exit(-1);
}

}  // namespace

proc_result dds::run_proc(const proc_options& opts) {
    if (opts.stdin_.empty()) {
        dds_log(debug, "Spawning subprocess: {}", quote_command(opts.command));
    } else {
        dds_log(debug,
                "Spawning subprocess: {}\n\tWith stdin: {}",
                quote_command(opts.command),
                opts.stdin_);
    }
    int  stdio_pipe[2] = {};
    auto rc            = ::pipe(stdio_pipe);
    check_rc(rc == 0, "Create stdio pipe for subprocess");

    int read_pipe  = stdio_pipe[0];
    int write_pipe = stdio_pipe[1];

    int proc_stdin_pipe[2] = {};
    rc                     = ::pipe(proc_stdin_pipe);
    check_rc(rc == 0, "Create stdin pipe for subprocess");
    int stdin_read_pipe  = proc_stdin_pipe[0];
    int stdin_write_pipe = proc_stdin_pipe[1];

    auto child = spawn_child(opts, write_pipe, read_pipe, stdin_write_pipe, stdin_read_pipe);

    ::close(stdin_read_pipe);
    ::close(write_pipe);

    {
        const char* stdin_cur = opts.stdin_.data();
        size_t      remaining = opts.stdin_.size();
        while (remaining > 0) {
            ssize_t num_written = ::write(stdin_write_pipe, stdin_cur, remaining);
            check_rc(num_written != -1, "Unable to write to stdin for subprocess");
            remaining -= num_written;
            stdin_cur += num_written;
        }
        ::close(stdin_write_pipe);
    }

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