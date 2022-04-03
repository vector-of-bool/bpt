#if !_WIN32

#include <bpt/util/output.hpp>

#include <unistd.h>

using namespace bpt;

void bpt::enable_ansi_console() noexcept {
    // unix consoles generally already support ANSI control chars by default
}

bool bpt::stdout_is_a_tty() noexcept { return ::isatty(STDOUT_FILENO) != 0; }

#endif
