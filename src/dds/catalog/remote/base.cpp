#include "./base.hpp"

#include <dds/package/id.hpp>
#include <dds/util/log.hpp>

#include <nlohmann/json.hpp>

using namespace dds;

void remote_listing_base::apply_transforms(path_ref root) const {
    for (const auto& tr : transforms) {
        tr.apply_to(root);
    }
}

void remote_listing_base::generate_auto_lib_files(const package_id& pid, path_ref root) const {
    if (auto_lib.has_value()) {
        dds_log(info, "Generating library data automatically");

        auto pkg_strm         = open(root / "package.json5", std::ios::binary | std::ios::out);
        auto man_json         = nlohmann::json::object();
        man_json["name"]      = pid.name;
        man_json["version"]   = pid.version.to_string();
        man_json["namespace"] = auto_lib->namespace_;
        pkg_strm << nlohmann::to_string(man_json);

        auto lib_strm    = open(root / "library.json5", std::ios::binary | std::ios::out);
        auto lib_json    = nlohmann::json::object();
        lib_json["name"] = auto_lib->name;
        lib_strm << nlohmann::to_string(lib_json);
    }
}
