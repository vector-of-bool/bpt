#pragma once

#include <neo/concepts.hpp>

#include <optional>
#include <string>

namespace bpt {

std::optional<std::string> getenv(const std::string& env) noexcept;

bool getenv_bool(const std::string& env) noexcept;

template <neo::invocable Func>
std::string getenv(const std::string& name, Func&& fn) noexcept(noexcept(fn())) {
    auto val = getenv(name);
    if (!val) {
        return std::string(fn());
    }
    return *val;
}

}  // namespace bpt
