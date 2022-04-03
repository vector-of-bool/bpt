#pragma once

#include <bpt/crs/cache.hpp>

namespace dds::cli {

struct options;

/**
 * @brief Open a CRS cache in the appropriate directory, will all requested
 * remote repositories synced and enabled.
 *
 * @param opts The options given by the user.
 */
dds::crs::cache open_ready_cache(const options& opts);

}  // namespace dds::cli
