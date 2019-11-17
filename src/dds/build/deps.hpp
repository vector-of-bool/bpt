#pragma once

#include <dds/toolchain/deps.hpp>
#include <dds/util/fs.hpp>

#include <string>
#include <string_view>

namespace dds {

class database;

deps_info parse_mkfile_deps_file(path_ref where);
deps_info parse_mkfile_deps_str(std::string_view str);

void update_deps_info(database& db, const deps_info&);

struct deps_rebuild_info {
    std::vector<fs::path> newer_inputs;
    std::string           previous_command;
    std::string           previous_command_output;
};

deps_rebuild_info
get_rebuild_info(database& db, path_ref output_path);

}  // namespace dds