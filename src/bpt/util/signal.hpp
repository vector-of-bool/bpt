#pragma once

#include <stdexcept>

namespace dds {

class user_cancelled : public std::exception {};

void install_signal_handlers() noexcept;

void notify_cancel() noexcept;
void reset_cancelled() noexcept;
bool is_cancelled() noexcept;
void cancellation_point();

}  // namespace dds