#pragma once

#include <dds/source.hpp>
#include <dds/toolchain.hpp>
#include <dds/util/fs.hpp>

#include <memory>
#include <optional>
#include <stdexcept>

namespace dds {

struct compile_failure : std::runtime_error {
    using runtime_error::runtime_error;
};

struct shared_compile_file_rules {
    struct rules_impl {
        std::vector<fs::path>    inc_dirs;
        std::vector<std::string> defs;
        bool                     enable_warnings = false;
    };

    std::shared_ptr<rules_impl> _impl = std::make_shared<rules_impl>();

public:
    shared_compile_file_rules() = default;

    auto& include_dirs() noexcept { return _impl->inc_dirs; }
    auto& include_dirs() const noexcept { return _impl->inc_dirs; }

    auto& defs() noexcept { return _impl->defs; }
    auto& defs() const noexcept { return _impl->defs; }

    auto& enable_warnings() noexcept { return _impl->enable_warnings; }
    auto& enable_warnings() const noexcept { return _impl->enable_warnings; }
};

struct compile_file_plan {
    shared_compile_file_rules rules;
    dds::source_file          source;
    std::string               qualifier;

    fs::path get_object_file_path(const toolchain& tc) const noexcept;
    void     compile(const toolchain& tc, path_ref out_prefix) const;
};

void execute_all(const std::vector<compile_file_plan>&,
                 const toolchain& tc,
                 int              n_jobs,
                 path_ref         out_prefix);

}  // namespace dds
