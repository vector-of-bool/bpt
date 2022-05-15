#include "./config.hpp"

#include <bpt/util/env.hpp>

bool bpt::config::defaults::enable_sqlite3_trace() { return bpt::getenv_bool("BPT_SQLITE3_TRACE"); }
