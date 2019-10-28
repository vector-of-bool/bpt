#pragma once

#include <dds/build/plan/library.hpp>

#include <string>
#include <vector>

namespace dds {

class package_plan {
    std::string               _name;
    std::string               _namespace;
    std::vector<library_plan> _libraries;

public:
    package_plan(std::string_view name, std::string_view namespace_)
        : _name(name)
        , _namespace(namespace_) {}

    void add_library(library_plan lp) { _libraries.emplace_back(std::move(lp)); }

    auto& name() const noexcept { return _name; }
    auto& namespace_() const noexcept { return _namespace; }
    auto& libraries() const noexcept { return _libraries; }
};

}  // namespace dds