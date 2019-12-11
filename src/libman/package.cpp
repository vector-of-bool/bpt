#include "./package.hpp"

#include <libman/fmt.hpp>
#include <libman/parse.hpp>

using namespace lm;

package package::from_file(path_ref fpath) {
    package ret;
    auto    pairs = parse_file(fpath);

    std::string           _type_;
    std::vector<fs::path> libraries;
    read(fmt::format("Reading package file '{}'", fpath.string()),
         pairs,
         read_required("Type", _type_),
         read_check_eq("Type", "Package"),
         read_required("Name", ret.name),
         read_required("Namespace", ret.namespace_),
         read_accumulate("Requires", ret.requires_),
         read_accumulate("Library", libraries));

    for (path_ref lib_path : libraries) {
        ret.libraries.push_back(library::from_file(fpath.parent_path() / lib_path));
    }

    return ret;
}
