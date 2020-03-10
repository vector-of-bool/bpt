#if !_WIN32

#include <dds/util/output.hpp>

#include <unistd.h>

using namespace dds;

bool dds::stdout_is_a_tty() noexcept { return ::isatty(STDOUT_FILENO) != 0; }

#endif
