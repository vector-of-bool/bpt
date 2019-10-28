#pragma once

#include <dds/build/plan/compile_file.hpp>
#include <dds/util/fs.hpp>

#include <set>
#include <tuple>
#include <vector>

namespace dds {

class library_plan;

struct test_failure {
    fs::path    executable_path;
    std::string output;
    int         retc;
};

class link_executable_plan {
    std::vector<fs::path> _input_libs;
    compile_file_plan     _main_compile;
    fs::path              _out_subdir;
    std::string           _name;

public:
    link_executable_plan(std::vector<fs::path> in_libs,
                         compile_file_plan     cfp,
                         path_ref              out_subdir,
                         std::string           name_)
        : _input_libs(std::move(in_libs))
        , _main_compile(std::move(cfp))
        , _out_subdir(out_subdir)
        , _name(std::move(name_)) {}

    auto& main_compile_file() const noexcept { return _main_compile; }

    fs::path calc_executable_path(const build_env& env) const noexcept;

    void link(const build_env&, const library_plan&) const;
    std::optional<test_failure> run_test(build_env_ref) const;

    bool is_test() const noexcept;
    bool is_app() const noexcept;
};

}  // namespace dds