#include <dds/util/output.hpp>

#if _WIN32

#include <windows.h>

void dds::enable_ansi_console() noexcept {
    auto stdio_console = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdio_console == INVALID_HANDLE_VALUE) {
        // Oh well...
        return;
    }
    DWORD mode = 0;
    if (!::GetConsoleMode(stdio_console, &mode)) {
        // Failed to get the mode?
        return;
    }
    // Set the bit!
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    ::SetConsoleMode(stdio_console, mode);
}

bool dds::stdout_is_a_tty() noexcept {
    // XXX: Newer Windows consoles support ANSI color, so this should be made smarter
    auto stdio_console = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdio_console == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD mode = 0;
    if (!::GetConsoleMode(stdio_console, &mode)) {
        // Failed to get the mode
        return false;
    }
    return (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

#endif