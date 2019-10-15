#ifndef DDS_UTIL_TEST_HPP_INCLUDED
#define DDS_UTIL_TEST_HPP_INCLUDED

#include <iostream>

namespace dds {

int S_failed_checks = 0;

struct requirement_failed {};

#define CHECK(...)                                                                                 \
    do {                                                                                           \
        if (!(__VA_ARGS__)) {                                                                      \
            ++::dds::S_failed_checks;                                                              \
            std::cerr << "Check failed at " << __FILE__ << ':' << __LINE__ << ": " << #__VA_ARGS__ \
                      << "\n";                                                                     \
        }                                                                                          \
    } while (0)

#define REQUIRE(...)                                                                               \
    do {                                                                                           \
        if (!(__VA_ARGS__)) {                                                                      \
            ++::dds::S_failed_checks;                                                              \
            std::cerr << "Check failed at " << __FILE__ << ':' << __LINE__ << ": " << #__VA_ARGS__ \
                      << "\n";                                                                     \
            throw ::dds::requirement_failed();                                                     \
        }                                                                                          \
    } while (0)

#define DDS_TEST_MAIN                                                                              \
    int main() {                                                                                   \
        try {                                                                                      \
            run_tests();                                                                           \
        } catch (const ::dds::requirement_failed&) {                                               \
            return ::dds::S_failed_checks;                                                         \
        } catch (const std::exception& e) {                                                        \
            std::cerr << "An unhandled exception occured: " << e.what() << '\n';                   \
            return 2;                                                                              \
        }                                                                                          \
        return ::dds::S_failed_checks;                                                             \
    }                                                                                              \
    static_assert(true)

}  // namespace dds

#endif  // DDS_UTIL_TEST_HPP_INCLUDED