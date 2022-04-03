#pragma once

#include <neo/fwd.hpp>

#include <concepts>
#include <variant>

namespace bpt {

/**
 * @brief Use as a base class to create a type that wraps a variant in an alternative interface
 *
 * @tparam Ts The variant alternatives that are supported by this type.
 */
template <typename... Ts>
class variant_wrapper {
    using variant_type = std::variant<Ts...>;

    variant_type _var;

public:
    // clang-format off
    template <typename Arg>
        requires std::constructible_from<variant_type, Arg&&>
    // Explicit unless the variant can implicitly construct from the argument
    explicit(!std::convertible_to<Arg&&, variant_type>)
    constexpr variant_wrapper(Arg&& arg)
        // Noexcept if the conversion is noexcept
        noexcept(std::is_nothrow_constructible_v<variant_type, Arg&&>)
        // Construct:
        : _var(NEO_FWD(arg)) {}
    // clang-format on

    constexpr variant_wrapper() noexcept = default;

    variant_wrapper(const variant_wrapper&) noexcept = default;
    variant_wrapper(variant_wrapper&&) noexcept      = default;

    variant_wrapper& operator=(const variant_wrapper&) noexcept = default;
    variant_wrapper& operator=(variant_wrapper&&) noexcept = default;

protected:
    constexpr decltype(auto) visit(auto&& func) const { return std::visit(func, _var); }
    constexpr decltype(auto) visit(auto&& func) { return std::visit(func, _var); }

    /**
     * @brief Check whether this wrapper currently holds the named alternative
     */
    template <typename T>
    constexpr bool is() const noexcept requires(std::same_as<T, Ts> || ...) {
        return std::holds_alternative<T>(_var);
    }

    /**
     * @brief Obtain the named alternative
     */
    template <typename T>
    constexpr T& as() & noexcept {
        return *std::get_if<T>(&_var);
    }

    template <typename T>
    constexpr const T& as() const& noexcept {
        return *std::get_if<T>(&_var);
    }

    template <typename T>
    constexpr T&& as() && noexcept {
        return std::move(*std::get_if<T>(&_var));
    }

    template <typename T>
    constexpr const T&& as() const&& noexcept {
        return std::move(*std::get_if<T>(&_var));
    }

    constexpr bool operator==(const variant_wrapper& other) const noexcept {
        return _var == other._var;
    }

    constexpr auto operator<=>(const variant_wrapper& other) const noexcept {
        return _var <=> other._var;
    }
};

}  // namespace bpt