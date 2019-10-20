#pragma once

#include <cstddef>

namespace dds {

namespace detail {

template <std::size_t I,
          typename A = std::nullptr_t,
          typename B = std::nullptr_t,
          typename C = std::nullptr_t,
          typename D = std::nullptr_t>
decltype(auto) nth_arg(A&& a[[maybe_unused]] = nullptr,
                       B&& b[[maybe_unused]] = nullptr,
                       C&& c[[maybe_unused]] = nullptr,
                       D&& d[[maybe_unused]] = nullptr) {
    if constexpr (I == 0) {
        return (A &&) a;
    } else if constexpr (I == 1) {
        return (B &&) b;
    } else if constexpr (I == 2) {
        return (C &&) c;
    } else if constexpr (I == 3) {
        return (D &&) d;
    }
}

}  // namespace detail

// Based on https://github.com/Quincunx271/TerseLambda
#define DDS_CTL(...)                                                                               \
    (auto&&... tl_args)->auto {                                                                    \
        [[maybe_unused]] auto&& _1 = ::dds::detail::nth_arg<0, decltype(tl_args)...>(              \
            static_cast<decltype(tl_args)&&>(tl_args)...);                                         \
        [[maybe_unused]] auto&& _2 = ::dds::detail::nth_arg<1, decltype(tl_args)...>(              \
            static_cast<decltype(tl_args)&&>(tl_args)...);                                         \
        [[maybe_unused]] auto&& _3 = ::dds::detail::nth_arg<2, decltype(tl_args)...>(              \
            static_cast<decltype(tl_args)&&>(tl_args)...);                                         \
        [[maybe_unused]] auto&& _4 = ::dds::detail::nth_arg<3, decltype(tl_args)...>(              \
            static_cast<decltype(tl_args)&&>(tl_args)...);                                         \
        static_assert(sizeof...(tl_args) <= 4);                                                    \
        return (__VA_ARGS__);                                                                      \
    }

#define DDS_TL [&] DDS_CTL

}  // namespace dds