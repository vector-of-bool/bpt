#include "./spdlog_user.hpp"

#include <spdlog/spdlog.h>

int main() {
    auto result = ::write_message();
    if (result != 42) {
        spdlog::critical(
            "The test library returned the wrong value (This is a REAL dds test failure, and is "
            "very unexpected)");
        return 1;
    }
    return 0;
}