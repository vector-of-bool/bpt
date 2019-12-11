#include "./signal.hpp"

#include <csignal>

namespace {

std::sig_atomic_t got_signal = 0;

void handle_signal(int sig) { got_signal = sig; }

}  // namespace

using namespace dds;

void dds::notify_cancel() noexcept { got_signal = SIGINT; }

void dds::install_signal_handlers() noexcept {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
}

bool dds::is_cancelled() noexcept { return got_signal != 0; }
void dds::cancellation_point() {
    if (is_cancelled()) {
        throw user_cancelled();
    }
}