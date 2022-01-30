#include "./library.hpp"

#include <dds/sdist/root.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/log.hpp>

#include <fansi/styled.hpp>
#include <libman/usage.hpp>
#include <neo/ranges.hpp>
#include <neo/tl.hpp>
#include <range/v3/action/push_back.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include <cassert>
#include <ranges>
#include <string>

using namespace dds;
using namespace fansi::literals;

library_plan library_plan::create(path_ref                    pkg_base,
                                  const crs::package_info&    pkg,
                                  const crs::library_info&    lib,
                                  const library_build_params& params) {
    fs::path out_dir   = params.out_subdir;
    auto     qual_name = neo::ufmt("{}/{}", pkg.name.str, lib.name.str);

    // Source files are kept in different groups:
    std::vector<source_file> app_sources;
    std::vector<source_file> test_sources;
    std::vector<source_file> lib_sources;
    std::vector<source_file> header_sources;
    std::vector<source_file> public_header_sources;

    // Collect the source for this library. This will look for any compilable sources in the
    // `src/` subdirectory of the library.
    auto src_dir = dds::source_root(pkg_base / lib.path / "src");
    if (src_dir.exists()) {
        // Sort each source file between the three source arrays, depending on
        // the kind of source that we are looking at.
        auto all_sources = src_dir.collect_sources();
        for (const auto& sfile : all_sources) {
            if (sfile.kind == source_kind::test) {
                test_sources.push_back(sfile);
            } else if (sfile.kind == source_kind::app) {
                app_sources.push_back(sfile);
            } else if (sfile.kind == source_kind::source) {
                lib_sources.push_back(sfile);
            } else if (sfile.kind == source_kind::header) {
                header_sources.push_back(sfile);
            } else {
                assert(sfile.kind == source_kind::header_impl);
            }
        }
    }

    auto include_dir = dds::source_root{pkg_base / lib.path / "include"};
    if (include_dir.exists()) {
        auto all_sources = include_dir.collect_sources();
        for (const auto& sfile : all_sources) {
            if (!is_header(sfile.kind)) {
                dds_log(
                    warn,
                    "Public include/ should only contain header files. Not a header: [.br.yellow[{}]]"_styled,
                    sfile.path.string());
            } else if (sfile.kind == source_kind::header) {
                public_header_sources.push_back(sfile);
            }
        }
    }
    if (!params.build_tests) {
        public_header_sources.clear();
        header_sources.clear();
    }

    ///  Predicate for if a '.kind' member is equal to the given kidn
    auto kind_is_for = [](crs::usage_kind k) { return [k](auto& l) { return l.kind == k; }; };

    auto pkg_dep_to_usages = [&](crs::dependency const& dep) {
        neo_assert(invariant,
                   dep.uses.is<crs::explicit_uses_list>(),
                   "Expected an explicit usage requirement for dependency object",
                   dep,
                   pkg,
                   lib);
        return dep.uses.as<crs::explicit_uses_list>().uses | std::views::transform([&](auto&& l) {
                   return lm::usage{dep.name.str, l.str};
               });
    };

    auto usages_of_kind = [&](crs::usage_kind k) -> neo::ranges::range_of<lm::usage> auto {
        auto intra = lib.intra_uses               //
            | std::views::filter(kind_is_for(k))  //
            | std::views::transform([&](auto& use) {
                         return lm::usage{pkg.namespace_.str, use.lib.str};
                     });
        auto from_dep                                   //
            = lib.dependencies                          //
            | std::views::filter(kind_is_for(k))        //
            | std::views::transform(pkg_dep_to_usages)  //
            | std::views::join;
        neo::ranges::range_of<lm::usage> auto ret_uses = ranges::views::concat(intra, from_dep);
        return ret_uses | neo::to_vector;
    };

    auto lib_uses  = usages_of_kind(crs::usage_kind::lib);
    auto test_uses = usages_of_kind(crs::usage_kind::test);
    //! auto app_uses  = usages_of_kind(crs::usage_kind::app);

    // Load up the compile rules
    shared_compile_file_rules compile_rules;
    auto&                     pub_inc = include_dir.exists() ? include_dir.path : src_dir.path;
    compile_rules.include_dirs().push_back(pub_inc);
    compile_rules.enable_warnings() = params.enable_warnings;
    extend(compile_rules.uses(), lib_uses);

    auto public_header_compile_rules          = compile_rules.clone();
    public_header_compile_rules.syntax_only() = true;
    public_header_compile_rules.defs().push_back("__dds_header_check=1");
    auto src_header_compile_rules = public_header_compile_rules.clone();
    if (include_dir.exists()) {
        compile_rules.include_dirs().push_back(src_dir.path);
        src_header_compile_rules.include_dirs().push_back(src_dir.path);
    }

    // Convert the library sources into their respective file compilation plans.
    auto lib_compile_files =  //
        lib_sources           //
        | ranges::views::transform([&](const source_file& sf) {
              return compile_file_plan(compile_rules, sf, qual_name, out_dir / "obj");
          })
        | ranges::to_vector;

    // Run a syntax-only pass over headers to verify that headers can build in isolation.
    auto header_indep_plan = header_sources  //
        | ranges::views::transform([&](const source_file& sf) {
                                 return compile_file_plan(src_header_compile_rules,
                                                          sf,
                                                          qual_name,
                                                          out_dir / "timestamps");
                             })
        | ranges::to_vector;
    extend(header_indep_plan,
           public_header_sources | ranges::views::transform([&](const source_file& sf) {
               return compile_file_plan(public_header_compile_rules,
                                        sf,
                                        qual_name,
                                        out_dir / "timestamps");
           }));
    // If we have any compiled library files, generate a static library archive
    // for this library
    std::optional<create_archive_plan> archive_plan;
    if (!lib_compile_files.empty()) {
        dds_log(debug, "Generating an archive library for {}", qual_name);
        archive_plan.emplace(lib.name.str, qual_name, out_dir, std::move(lib_compile_files));
    } else {
        dds_log(debug,
                "Library {} has no compiled inputs, so no archive will be generated",
                qual_name);
    }

    // Collect the paths to linker inputs that should be used when generating executables for
    // this library.
    std::vector<lm::usage> links;
    extend(links, lib_uses);

    // There may also be additional usage requirements for tests
    auto test_rules = compile_rules.clone();
    auto test_links = links;
    extend(test_rules.uses(), test_uses);
    extend(test_links, test_uses);

    // Generate the plans to link any executables for this library
    std::vector<link_executable_plan> link_executables;
    for (const source_file& source : ranges::views::concat(app_sources, test_sources)) {
        const bool is_test = source.kind == source_kind::test;
        if (is_test && !params.build_tests) {
            // This is a test, but we don't want to build tests
            continue;
        }
        if (!is_test && !params.build_apps) {
            // This is an app, but we don't want to build apps
            continue;
        }
        // Pick a subdir based on app/test
        const auto subdir_base = is_test ? out_dir / "test" : out_dir;
        // Put test/app executables in a further subdirectory based on the source file path
        const auto subdir = subdir_base / source.relative_path().parent_path();
        // Pick compile rules based on app/test
        auto rules = is_test ? test_rules : compile_rules;
        // Pick input libs based on app/test
        auto& exe_links = is_test ? test_links : links;
        // TODO: Apps/tests should only see the _public_ include dir, not both
        auto exe
            = link_executable_plan{exe_links,
                                   compile_file_plan(rules, source, qual_name, out_dir / "obj"),
                                   subdir,
                                   source.path.stem().stem().string()};
        link_executables.emplace_back(std::move(exe));
    }

    // Done!
    return library_plan{lib.name.str,
                        pkg_base / lib.path,
                        qual_name,
                        out_dir,
                        std::move(archive_plan),
                        std::move(link_executables),
                        std::move(header_indep_plan),
                        std::move(lib_uses)};
}

fs::path library_plan::public_include_dir() const noexcept {
    auto p = source_root() / "include";
    if (fs::exists(p)) {
        return p;
    }
    return source_root() / "src";
}
