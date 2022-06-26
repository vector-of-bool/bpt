#include "./signal.hpp"

#include <csignal>

namespace {

std::sig_atomic_t got_signal = 0;

void handle_signal(int sig) { got_signal = sig; }

}  // namespace

using namespace bpt;

void bpt::notify_cancel() noexcept { got_signal = SIGINT; }

void bpt::install_signal_handlers() noexcept {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

#ifdef SIGQUIT
    // Some systems issue SIGQUIT :shrug:
    std::signal(SIGQUIT, handle_signal);
#endif

#ifdef SIGPIPE
    // XXX: neo-io doesn't behave nicely when EOF is hit on sockets. This Isn't
    // easily fixed portably without simply blocking SIGPIPE globally.
    std::signal(SIGPIPE, SIG_IGN);
#endif
}

bool bpt::is_cancelled() noexcept { return got_signal != 0; }
void bpt::cancellation_point() {
    if (is_cancelled()) {
        throw user_cancelled();
    }
}
