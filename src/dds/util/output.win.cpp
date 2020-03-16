#if _WIN32

#include <dds/util/output.hpp>

bool dds::stdout_is_a_tty() noexcept {
    // XXX: Newer Windows consoles support ANSI color, so this should be made smarter
    return false;
}

#endif