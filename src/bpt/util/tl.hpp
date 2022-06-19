#pragma once

#include <neo/tl.hpp>

#define BPT_CTL(...)                                                                               \
    <typename... TlArgs>(TlArgs && ... _args)                                                      \
        ->decltype(auto) requires(NEO_TL_REQUIRES(__VA_ARGS__)) {                                  \
        [[maybe_unused]] auto&& _1 = ::neo::tl_detail::nth_arg<0>(NEO_FWD(_args)...);              \
        [[maybe_unused]] auto&& _2 = ::neo::tl_detail::nth_arg<1>(NEO_FWD(_args)...);              \
        [[maybe_unused]] auto&& _3 = ::neo::tl_detail::nth_arg<2>(NEO_FWD(_args)...);              \
        [[maybe_unused]] auto&& _4 = ::neo::tl_detail::nth_arg<3>(NEO_FWD(_args)...);              \
        return (__VA_ARGS__);                                                                      \
    }

#define BPT_TL [&] BPT_CTL
