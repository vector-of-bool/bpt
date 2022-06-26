#pragma once

#include <bpt.tweaks.hpp>

namespace bpt::config {

namespace defaults {

/**
 * @brief Control whether --log-level=trace writes log events for SQLite statement step()
 * invocations. This default returns true if the BPT_SQLITE3_TRACE environment variable is
 * set to a truthy value.
 */
bool enable_sqlite3_trace();

}  // namespace defaults

using namespace defaults;

}  // namespace bpt::config
