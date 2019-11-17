#pragma once

#include <dds/build/plan/base.hpp>
#include <dds/source.hpp>
#include <dds/toolchain/deps.hpp>

#include <memory>

namespace dds {

struct compile_failure : std::runtime_error {
    using runtime_error::runtime_error;
};

class shared_compile_file_rules {
    struct rules_impl {
        std::vector<fs::path>    inc_dirs;
        std::vector<std::string> defs;
        bool                     enable_warnings = false;
    };

    std::shared_ptr<rules_impl> _impl = std::make_shared<rules_impl>();

public:
    shared_compile_file_rules() = default;

    auto clone() const noexcept {
        auto cp  = *this;
        cp._impl = std::make_shared<rules_impl>(*_impl);
        return cp;
    }

    auto& include_dirs() noexcept { return _impl->inc_dirs; }
    auto& include_dirs() const noexcept { return _impl->inc_dirs; }

    auto& defs() noexcept { return _impl->defs; }
    auto& defs() const noexcept { return _impl->defs; }

    auto& enable_warnings() noexcept { return _impl->enable_warnings; }
    auto& enable_warnings() const noexcept { return _impl->enable_warnings; }
};

class compile_file_plan {
    shared_compile_file_rules _rules;
    source_file               _source;
    std::string               _qualifier;
    fs::path                  _subdir;

public:
    compile_file_plan(shared_compile_file_rules rules,
                      source_file               sf,
                      std::string_view          qual,
                      path_ref                  subdir)
        : _rules(rules)
        , _source(std::move(sf))
        , _qualifier(qual)
        , _subdir(subdir) {}

    compile_command_info generate_compile_command(build_env_ref) const noexcept;

    const source_file& source() const noexcept { return _source; }
    path_ref           source_path() const noexcept { return _source.path; }

    fs::path                 calc_object_file_path(build_env_ref env) const noexcept;
    std::optional<deps_info> compile(build_env_ref) const;
};

}  // namespace dds