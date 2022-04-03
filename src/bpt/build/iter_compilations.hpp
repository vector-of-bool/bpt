#pragma once

#include <bpt/build/plan/full.hpp>

#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>

namespace dds {

/**
 * Iterate over every library defined as part of the build plan
 */
inline auto iter_libraries(const build_plan& plan) {
    return                                                    //
        plan.packages()                                       //
        | ranges::views::transform(&package_plan::libraries)  //
        | ranges::views::join                                 //
        ;
}

/**
 * Return a range iterating over ever file compilation defined in the given build plan
 */
inline auto iter_compilations(const build_plan& plan) {
    auto lib_compiles =                                                 //
        iter_libraries(plan)                                            //
        | ranges::views::transform(&library_plan::archive_plan)         //
        | ranges::views::filter([&](auto&& opt) { return bool(opt); })  //
        | ranges::views::transform([&](auto&& opt) -> auto& {
              return opt->file_compilations();
          })                   //
        | ranges::views::join  //
        ;

    auto header_compiles =                                  //
        iter_libraries(plan)                                //
        | ranges::views::transform(&library_plan::headers)  //
        | ranges::views::join;

    auto exe_compiles =                                                       //
        iter_libraries(plan)                                                  //
        | ranges::views::transform(&library_plan::executables)                //
        | ranges::views::join                                                 //
        | ranges::views::transform(&link_executable_plan::main_compile_file)  //
        ;

    return ranges::views::concat(lib_compiles, header_compiles, exe_compiles);
}

}  // namespace dds