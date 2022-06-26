#pragma once

#include <boost/leaf.hpp>
#include <boost/leaf/context.hpp>

#include <tuple>
#include <type_traits>

namespace bpt {

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
        static_assert(has_handlers, "bpt_leaf_try requires one or more bpt_leaf_catch blocks");
        return std::apply(
            [&](auto&... hs) {
                if constexpr (handle_all.value) {
                    if constexpr (boost::leaf::is_result_type<result_type>::value) {
                        return boost::leaf::try_handle_all(try_block, hs...);
                    } else {
                        return boost::leaf::try_catch(try_block, hs...);
                    }
                } else {
                    if constexpr (boost::leaf::is_result_type<result_type>::value) {
                        return boost::leaf::try_handle_some(try_block, hs...);
                    } else {
                        return boost::leaf::try_handle_some(
                            [&]() -> boost::leaf::result<result_type> { return try_block(); },
                            hs...);
                    }
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

}  // namespace bpt

/**
 * @brief Create a try {} block that handles all errors using Boost.LEAF
 */
#define bpt_leaf_try                                                                               \
    ::bpt::leaf_exec_try_catch_sequence<true>{} + ::bpt::leaf_make_try_block{}->*[&]()

/**
 * @brief Create a try {} block that handles errors using Boost.LEAF
 *
 * The result is always a result type.
 */
#define bpt_leaf_try_some                                                                          \
    ::bpt::leaf_exec_try_catch_sequence<false>{} + ::bpt::leaf_make_try_block{}->*[&]()

/**
 * @brief Create an error handling block for Boost.LEAF
 */
#define bpt_leaf_catch *::bpt::leaf_make_catch_block{}->*[&]

#define bpt_leaf_catch_all                                                                         \
    bpt_leaf_catch(::boost::leaf::verbose_diagnostic_info const& diagnostic_info [[maybe_unused]])
