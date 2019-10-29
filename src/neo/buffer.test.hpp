#pragma once

#include <iostream>

#define CHECK(...)                                                                                 \
    do {                                                                                           \
        if (!(__VA_ARGS__)) {                                                                      \
            std::cerr << "Check failed: " << (#__VA_ARGS__) << '\n';                               \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
