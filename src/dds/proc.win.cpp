#ifdef _WIN32
#include "./proc.hpp"

#include <dds/util/fs.hpp>
#include <dds/util/log.hpp>

#include <fmt/core.h>
#include <neo/assert.hpp>
#include <wil/resource.h>

#include <windows.h>

#include <cassert>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace dds;
using namespace std::chrono_literals;

namespace {

[[noreturn]] void throw_system_error(std::string what) {
    throw std::system_error(std::error_code(::GetLastError(), std::system_category()), what);
}

std::wstring widen(std::string_view s) {
    if (s.empty()) {
        return L"";
    }
    auto req_chars
        = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring ret;
    ret.resize(req_chars);
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), ret.data(), req_chars);
    return ret;
}

}  // namespace

proc_result dds::run_proc(const proc_options& opts) {
    auto cmd_str  = quote_command(opts.command);
    auto cmd_wide = widen(cmd_str);
    dds_log(debug, "Spawning subprocess: {}", cmd_str);

    ::SECURITY_ATTRIBUTES security = {};
    security.bInheritHandle        = TRUE;
    security.nLength               = sizeof security;
    security.lpSecurityDescriptor  = nullptr;
    wil::unique_hfile reader;
    wil::unique_hfile writer;
    auto              okay = ::CreatePipe(&reader, &writer, &security, 0);
    if (!okay) {
        throw_system_error("Failed to create a stdio pipe");
    }

    ::SetHandleInformation(reader.get(), HANDLE_FLAG_INHERIT, 0);
    ::COMMTIMEOUTS timeouts;
    ::GetCommTimeouts(reader.get(), &timeouts);

    wil::unique_process_information proc_info;

    ::STARTUPINFOW startup_info = {};
    ::RtlSecureZeroMemory(&startup_info, sizeof startup_info);
    startup_info.hStdOutput = startup_info.hStdError = writer.get();
    startup_info.dwFlags                             = STARTF_USESTDHANDLES;
    startup_info.cb                                  = sizeof startup_info;
    // DO IT!
    okay = ::CreateProcessW(nullptr,  // cmd[0].data(),
                            cmd_wide.data(),
                            nullptr,
                            nullptr,
                            true,
                            CREATE_NEW_PROCESS_GROUP,
                            nullptr,
                            opts.cwd.value_or(fs::current_path()).c_str(),
                            &startup_info,
                            &proc_info);
    if (!okay) {
        throw_system_error(fmt::format("Failed to spawn child process [{}]", cmd_str));
    }

    writer.reset();

    std::string output;
    proc_result res;

    auto timeout = opts.timeout;
    while (true) {
        const int buffer_size = 256;
        char      buffer[buffer_size];
        DWORD     nread = 0;
        // Reload the timeout on the pipe
        timeouts.ReadTotalTimeoutConstant = static_cast<DWORD>(timeout.value_or(0ms).count());
        ::SetCommTimeouts(reader.get(), &timeouts);
        // Read some bytes from the process
        okay = ::ReadFile(reader.get(), buffer, buffer_size, &nread, nullptr);
        if (!okay && ::GetLastError() == ERROR_TIMEOUT) {
            // We didn't read any bytes. Hit the timeout
            neo_assert_always(invariant,
                              nread == 0,
                              "Didn't expect to read bytes when a timeout was reached",
                              nread,
                              timeout->count());
            res.timed_out = true;
            timeout       = std::nullopt;
            ::GenerateConsoleCtrlEvent(CTRL_C_EVENT, proc_info.dwProcessId);
            continue;
        }
        if (!okay && ::GetLastError() != ERROR_BROKEN_PIPE) {
            throw_system_error("Failed while reading from the stdio pipe");
        }
        output.append(buffer, buffer + nread);
        if (nread == 0) {
            break;
        }
    }

    ::WaitForSingleObject(proc_info.hProcess, INFINITE);

    DWORD rc = 0;
    okay     = ::GetExitCodeProcess(proc_info.hProcess, &rc);

    if (!okay || rc == STILL_ACTIVE) {
        throw_system_error("Failed reading exit code of process");
    }

    res.retc   = rc;
    res.output = std::move(output);
    return res;
}

#endif  // _WIN32