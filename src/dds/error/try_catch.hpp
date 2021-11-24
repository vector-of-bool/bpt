#pragma once

#include <boost/leaf.hpp>
#include <boost/leaf/context.hpp>

#include <tuple>
#include <type_traits>

namespace dds {

struct leaf_try_block_lhs {};

template <typename Try, typename... Handlers>
struct leaf_handler_seq {
    using result_type = std::invoke_result_t<Try>;
    Try& try_block;

    std::tuple<Handlers&...> handlers{};

    template <typename Catch>
    constexpr auto operator*(Catch c) const noexcept {
        return leaf_handler_seq<Try, Handlers..., typename Catch::handler_type>{
            try_block, std::tuple_cat(handlers, std::tie(c.h))};
    }

    constexpr decltype(auto) invoke(auto handle_all) const {
        constexpr auto has_handlers = sizeof...(Handlers) != 0;
        static_assert(has_handlers, "dds_leaf_try requires one or more dds_leaf_catch blocks");
        return std::apply(
            [&](auto&... hs) {
                if constexpr (handle_all.value) {
                    if constexpr (boost::leaf::is_result_type<result_type>::value) {
                        return boost::leaf::try_handle_all(try_block, hs...);
                    } else {
                        return boost::leaf::try_catch(try_block, hs...);
                    }
                } else {
                    return boost::leaf::try_handle_some(try_block, hs...);
                }
            },
            handlers);
    }
};

struct leaf_make_try_block {
    template <typename Func>
    constexpr decltype(auto) operator->*(Func&& block) const {
        return leaf_handler_seq<Func>{block};
    }
};

template <typename H>
struct leaf_catch_block {
    using handler_type = H;

    handler_type& h;

    leaf_catch_block(H& h)
        : h(h) {}
};

struct leaf_make_catch_block {
    template <typename Func>
    constexpr decltype(auto) operator->*(Func&& block) const {
        return leaf_catch_block<std::remove_cvref_t<Func>>{block};
    }
};

template <bool HandleAll>
struct leaf_exec_try_catch_sequence {
    template <typename Try, typename... Handlers>
    constexpr decltype(auto) operator+(const leaf_handler_seq<Try, Handlers...> seq) const {
        return seq.invoke(std::bool_constant<HandleAll>{});
    }
};

using boost::leaf::catch_;

struct noreturn_t {
    template <typename T>
    constexpr operator T() const noexcept {
        std::terminate();
    }
};

}  // namespace dds

#define dds_leaf_try                                                                               \
    ::dds::leaf_exec_try_catch_sequence<true>{} + ::dds::leaf_make_try_block{}->*[&]()
#define dds_leaf_try_some                                                                          \
    ::dds::leaf_exec_try_catch_sequence<false>{} + ::dds::leaf_make_try_block{}->*[&]()

#define dds_leaf_catch *::dds::leaf_make_catch_block{}->*[&]

#define dds_leaf_catch_all                                                                         \
    dds_leaf_catch(::boost::leaf::verbose_diagnostic_info const& diagnostic_info [[maybe_unused]])

#define DDS_E_THROWING(...)                                                                        \
    boost::leaf::try_catch([&] { return (__VA_ARGS__); },                                          \
                           [](::boost::leaf::catch_<std::exception>) -> ::dds::noreturn_t {        \
                               BOOST_LEAF_NEW_ERROR();                                             \
                               throw;                                                              \
                           })
