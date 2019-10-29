#include "./library.hpp"

#include <dds/util/algo.hpp>

#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>

using namespace dds;

library_plan library_plan::create(const library&               lib,
                                  const library_build_params&  params,
                                  const usage_requirement_map& ureqs) {
    std::vector<compile_file_plan>     compile_files;
    std::vector<link_executable_plan>  link_executables;
    std::optional<create_archive_plan> create_archive;

    std::vector<source_file> app_sources;
    std::vector<source_file> test_sources;
    std::vector<source_file> lib_sources;

    auto src_dir = lib.src_dir();
    if (src_dir.exists()) {
        auto all_sources = src_dir.sources();
        auto to_compile  = all_sources | ranges::views::filter([&](const source_file& sf) {
                              return (sf.kind == source_kind::source
                                      || (sf.kind == source_kind::app && params.build_apps)
                                      || (sf.kind == source_kind::test && params.build_tests));
                          });

        for (const auto& sfile : to_compile) {
            if (sfile.kind == source_kind::test) {
                test_sources.push_back(sfile);
            } else if (sfile.kind == source_kind::app) {
                app_sources.push_back(sfile);
            } else {
                lib_sources.push_back(sfile);
            }
        }
    }

    auto compile_rules              = lib.base_compile_rules();
    compile_rules.enable_warnings() = params.enable_warnings;
    for (const auto& use : lib.manifest().uses) {
        ureqs.apply(compile_rules, use.namespace_, use.name);
    }

    for (const auto& sf : lib_sources) {
        compile_files.emplace_back(compile_rules,
                                   sf,
                                   lib.manifest().name,
                                   params.out_subdir / "obj");
    }

    if (!lib_sources.empty()) {
        create_archive.emplace(lib.manifest().name, params.out_subdir, std::move(compile_files));
    }

    std::vector<fs::path> in_libs;
    for (auto& use : lib.manifest().uses) {
        extend(in_libs, ureqs.link_paths(use.namespace_, use.name));
    }
    for (auto& link : lib.manifest().links) {
        extend(in_libs, ureqs.link_paths(link.namespace_, link.name));
    }

    for (const source_file& source : ranges::views::concat(app_sources, test_sources)) {
        auto subdir
            = source.kind == source_kind::test ? params.out_subdir / "test" : params.out_subdir;
        link_executables.emplace_back(in_libs,
                                      compile_file_plan(compile_rules,
                                                        source,
                                                        lib.manifest().name,
                                                        params.out_subdir / "obj"),
                                      subdir,
                                      source.path.stem().stem().string());
    }

    return library_plan{lib.manifest().name,
                        lib.path(),
                        std::move(create_archive),
                        std::move(link_executables),
                        lib.manifest().uses,
                        lib.manifest().links};
}