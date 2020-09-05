#include "./index.hpp"

#include <libman/parse.hpp>

#include <fmt/core.h>

using namespace lm;

lm::index index::from_file(path_ref fpath) {
    const auto kvs = parse_file(fpath);
    // const auto type = kvs.find("Type");
    // if (!type || type->value() != "Index") {
    //     throw std::runtime_error(
    //         fmt::format("Libman file has missing/incorrect 'Type' ({})", fpath.string()));
    // }

    index ret;

    std::optional<std::string> type;
    std::vector<std::string>   package_lines;

    read(fmt::format("Reading libman index file '{}'", fpath.string()),
         kvs,
         read_required("Type", type),
         read_check_eq("Type", "Index"),
         read_accumulate("Package", package_lines));

    for (const auto& pkg_line : package_lines) {
        auto items = dds::split(pkg_line, ";");
        std::transform(items.begin(), items.end(), items.begin(), [](auto s) {
            return std::string(trim_view(s));
        });
        if (items.size() != 2) {
            throw std::runtime_error(
                fmt::format("Invalid 'Package' field in index file ({}): 'Package: {}'",
                            fpath.string(),
                            pkg_line));
        }

        auto pkg = package::from_file(fpath.parent_path() / items[1]);
        if (pkg.name != items[0]) {
            // throw std::runtime_error(fmt::format(
            //     "Package file ({}) listed different name '{}' than the index file '{}'",
            //     items[1],
            //     pkg.name,
            //     items[0]));
        }
        ret.packages.push_back(std::move(pkg));
    }

    return ret;
}

index::library_index index::build_library_index() const {
    library_index ret;
    for (auto& pkg : packages) {
        for (auto& lib : pkg.libraries) {
            auto pair       = std::pair(pkg.namespace_, lib.name);
            bool did_insert = ret.try_emplace(pair, lib).second;
            if (!did_insert) {
                throw std::runtime_error(
                    fmt::format("Duplicate library '{}/{}' defined in the libman tree",
                                pair.first,
                                pair.second));
            }
        }
    }
    return ret;
}