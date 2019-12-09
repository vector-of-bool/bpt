#pragma once

#include <dds/util/fs.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace dds {

enum class file_deps_mode {
    none,
    msvc,
    gnu,
};

struct file_deps_info {
    fs::path              output;
    std::vector<fs::path> inputs;
    std::string           command;
    std::string           command_output;
};

class database;

file_deps_info parse_mkfile_deps_file(path_ref where);
file_deps_info parse_mkfile_deps_str(std::string_view str);

struct msvc_deps_info {
    struct file_deps_info deps_info;
    std::string           cleaned_output;
};

msvc_deps_info parse_msvc_output_for_deps(std::string_view output, std::string_view leader);

void update_deps_info(database& db, const file_deps_info&);

struct deps_rebuild_info {
    std::vector<fs::path> newer_inputs;
    std::string           previous_command;
    std::string           previous_command_output;
};

deps_rebuild_info get_rebuild_info(const database& db, path_ref output_path);

}  // namespace dds