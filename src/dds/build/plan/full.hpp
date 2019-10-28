#pragma once

#include <dds/build/plan/exe.hpp>
#include <dds/build/plan/package.hpp>

namespace dds {

class build_plan {
    std::vector<package_plan> _packages;

public:
    package_plan& add_package(package_plan p) noexcept {
        return _packages.emplace_back(std::move(p));
    }
    auto& packages() const noexcept { return _packages; }
    void  compile_all(const build_env& env, int njobs) const;
    void  archive_all(const build_env& env, int njobs) const;
    void  link_all(const build_env& env, int njobs) const;

    std::vector<test_failure> run_all_tests(build_env_ref env, int njobs) const;
};

}  // namespace dds
