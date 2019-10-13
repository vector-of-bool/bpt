#ifdef _WIN32
#include "./proc.hpp"

#include <dds/logging.hpp>

#include <wil/resource.h>

#include <windows.h>

#include <cassert>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace dds;

namespace {

[[noreturn]] void throw_system_error(const char* what) {
    throw std::system_error(std::error_code(::GetLastError(), std::system_category()), what);
}

std::string concat_args(const std::vector<std::string>& cmd) {
    std::stringstream strm;
    for (const auto& arg : cmd) {
        strm << arg << ' ';
    }
    return strm.str();
}

}  // namespace

proc_result dds::run_proc(const std::vector<std::string>& cmd) {
    auto cmd_str = concat_args(cmd);

    ::SECURITY_ATTRIBUTES security = {};
    security.bInheritHandle = TRUE;
    security.nLength = sizeof security;
    security.lpSecurityDescriptor = nullptr;
    wil::unique_hfile reader;
    wil::unique_hfile writer;
    auto okay = ::CreatePipe(&reader, &writer, &security, 0);
    if (!okay) {
        throw_system_error("Failed to create a stdio pipe");
    }

    ::SetHandleInformation(reader.get(), HANDLE_FLAG_INHERIT, 0);

    wil::unique_process_information proc_info;

    ::STARTUPINFOA startup_info = {};
    ::RtlSecureZeroMemory(&startup_info, sizeof startup_info);
    startup_info.hStdOutput = startup_info.hStdError = writer.get();
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.cb = sizeof startup_info;
    okay = ::CreateProcessA(nullptr,  // cmd[0].data(),
                            cmd_str.data(),
                            nullptr,
                            nullptr,
                            true,
                            0,
                            nullptr,
                            nullptr,
                            &startup_info,
                            &proc_info);
    if (!okay) {
        throw_system_error("Failed to spawn a child process");
    }

    writer.reset();

    std::string output;
    while (true) {
        const int buffer_size = 256;
        char buffer[buffer_size];
        DWORD nread = 0;
        okay = ::ReadFile(reader.get(), buffer, buffer_size, &nread, nullptr);
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
    okay = ::GetExitCodeProcess(proc_info.hProcess, &rc);

    if (!okay || rc == STILL_ACTIVE) {
        throw_system_error("Failed reading exit code of process");
    }

    proc_result res;
    res.retc = rc;
    res.output = std::move(output);
    return res;
}

#endif  // _WIN32