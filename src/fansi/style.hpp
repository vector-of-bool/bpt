#pragma once

#include <string>

namespace fansi {

enum class std_color {
    unspecified = -1,
    black       = 0,
    red         = 1,
    green       = 2,
    yellow      = 3,
    blue        = 4,
    magent      = 5,
    cyan        = 6,
    white       = 7,
    normal      = 9,
};

struct text_style {
    std_color fg_color = std_color::normal;
    std_color bg_color = std_color::normal;

    bool bright    = false;
    bool bold      = false;
    bool faint     = false;
    bool italic    = false;
    bool underline = false;
    bool reverse   = false;
    bool strike    = false;
};

bool detect_should_style() noexcept;

}  // namespace fansi