#pragma once

#include <dds/build/plan/full.hpp>
#include <dds/util/range_compat.hpp>

#include <range/v3/view/concat.hpp>

#include <neo/ranges.hpp>
#include <neo/tl.hpp>

namespace dds {

/**
 * Iterate over every library defined as part of the build plan
 */
inline auto iter_libraries(const build_plan& plan) {
    return                                               //
        plan.packages()                                  //
        | std::views::transform(NEO_TL(_1.libraries()))  //
        | std::views::join                               //
        ;
}

/**
 * Return a range iterating over ever file compilation defined in the given build plan
 */
inline auto iter_compilations(const build_plan& plan) {

    auto lib_compiles =                                           //
        iter_libraries(plan)                                      //
        | std::views::transform(NEO_TL(_1.archive_plan()))        //
        | std::views::filter(NEO_TL(_1.has_value()))              //
        | std::views::transform(NEO_TL(_1->file_compilations()))  //
        | std::views::join                                        //
        ;

    auto exe_compiles =                                          //
        iter_libraries(plan)                                     //
        | std::views::transform(NEO_TL(_1.executables()))        //
        | std::views::join                                       //
        | std::views::transform(NEO_TL(_1.main_compile_file()))  //
        ;

    return ranges::views::concat(lib_compiles, exe_compiles);
}

}  // namespace dds
