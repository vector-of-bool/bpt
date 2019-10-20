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

class compilation_rules {
    struct rules_impl {
        std::vector<fs::path>    inc_dirs;
        std::vector<std::string> defs;
        fs::path                 base_path;
    };

    std::shared_ptr<rules_impl> _impl = std::make_shared<rules_impl>();

public:
    compilation_rules() = default;

    auto&       base_path() noexcept { return _impl->base_path; }
    const auto& base_path() const noexcept { return _impl->base_path; }

    auto&       include_dirs() noexcept { return _impl->inc_dirs; }
    const auto& include_dirs() const noexcept { return _impl->inc_dirs; }

    auto&       defs() noexcept { return _impl->defs; }
    const auto& defs() const noexcept { return _impl->defs; }
};

struct file_compilation {
    compilation_rules rules;
    source_file       source;
    fs::path          obj;
    std::string       owner_name;
    bool              enable_warnings = false;

    file_compilation(compilation_rules rules,
                     source_file       sf,
                     path_ref          obj,
                     std::string       owner,
                     bool);

    void compile(const toolchain& tc) const;
};

void execute_all(const std::vector<file_compilation>&, const toolchain& tc, int n_jobs);

}  // namespace dds