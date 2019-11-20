#pragma once

namespace semver {

enum class order {
    less = -1,
    equivalent = 0,
    greater = 1,
};

} // namespace semver