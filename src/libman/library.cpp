#include "./library.hpp"

#include <bpt/error/result.hpp>
#include <bpt/util/copy.hpp>
#include <bpt/util/tl.hpp>

#include <libman/parse.hpp>

#include <fmt/core.h>

using namespace lm;

bpt::result<library> library::from_file(path_ref fpath) {
    auto pairs = parse_file(fpath);

    library ret;

    std::string _type_;
    try {
        read(fmt::format("Reading library manifest file '{}'", fpath.string()),
             pairs,
             read_required("Type", _type_),
             read_check_eq("Type", "Library"),
             read_required("Name", ret.name),
             read_opt("Path", ret.linkable_path),
             read_accumulate("Include-Path", ret.include_paths),
             read_accumulate("Preprocessor-Define", ret.preproc_defs),
             read_accumulate("Uses",
                             ret.uses,
                             BPT_TL(bpt::decay_copy(split_usage_string(_1).value()))),
             read_accumulate("Links",
                             ret.links,
                             BPT_TL(bpt::decay_copy(split_usage_string(_1).value()))),
             read_accumulate("Special-Uses", ret.special_uses));
    } catch (const boost::leaf::bad_result& err) {
        return err.load();
    }

    auto make_absolute = [&](path_ref p) { return fpath.parent_path() / p; };
    std::transform(ret.include_paths.begin(),
                   ret.include_paths.end(),
                   ret.include_paths.begin(),
                   make_absolute);

    if (ret.linkable_path) {
        ret.linkable_path = make_absolute(*ret.linkable_path);
    }

    return ret;
}
