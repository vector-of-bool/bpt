#include "./file_deps.hpp"

#include <dds/db/database.hpp>
#include <dds/proc.hpp>
#include <dds/util/shlex.hpp>
#include <dds/util/string.hpp>

#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

using namespace dds;

file_deps_info dds::parse_mkfile_deps_file(path_ref where) {
    auto content = slurp_file(where);
    return parse_mkfile_deps_str(content);
}

file_deps_info dds::parse_mkfile_deps_str(std::string_view str) {
    file_deps_info ret;

    // Remove escaped newlines
    auto no_newlines = replace(str, "\\\n", " ");

    auto split = split_shell_string(str);
    auto iter  = split.begin();
    auto stop  = split.end();
    if (iter == stop) {
        spdlog::critical(
            "Invalid deps listing. Shell split was empty. This is almost certainly a bug.");
        return ret;
    }
    auto& head = *iter;
    ++iter;
    if (!ends_with(head, ":")) {
        spdlog::critical(
            "Invalid deps listing. Leader item is not colon-terminated. This is probably a bug. "
            "(Are you trying to use C++ Modules? That's not ready yet, sorry. Set `Deps-Mode` to "
            "`None` in your toolchain file.)");
        return ret;
    }
    ret.output = head.substr(0, head.length() - 1);
    ret.inputs.insert(ret.inputs.end(), iter, stop);
    return ret;
}

msvc_deps_info dds::parse_msvc_output_for_deps(std::string_view output, std::string_view leader) {
    auto           lines = split_view(output, "\n");
    std::string    cleaned_output;
    file_deps_info deps;
    for (const auto full_line : lines) {
        auto trimmed = trim_view(full_line);
        if (!starts_with(trimmed, leader)) {
            cleaned_output += std::string(full_line);
            cleaned_output.push_back('\n');
            continue;
        }
        auto remaining = trim_view(trimmed.substr(leader.size()));
        deps.inputs.emplace_back(fs::weakly_canonical(remaining));
    }
    if (!cleaned_output.empty()) {
        // Remove the extra newline at the back
        cleaned_output.pop_back();
    }
    return {deps, cleaned_output};
}

void dds::update_deps_info(database& db, const file_deps_info& deps) {
    db.store_file_command(deps.output, {deps.command, deps.command_output});
    db.forget_inputs_of(deps.output);
    for (auto&& inp : deps.inputs) {
        auto mtime = fs::last_write_time(inp);
        db.record_dep(inp, deps.output, mtime);
    }
}

deps_rebuild_info dds::get_rebuild_info(const database& db, path_ref output_path) {
    std::unique_lock lk{db.mutex()};
    auto             cmd_ = db.command_of(output_path);
    if (!cmd_) {
        return {};
    }
    auto& cmd     = *cmd_;
    auto  inputs_ = db.inputs_of(output_path);
    if (!inputs_) {
        return {};
    }
    auto& inputs        = *inputs_;
    auto  changed_files =  //
        inputs             //
        | ranges::views::filter([](const input_file_info& input) {
              return !fs::exists(input.path) || fs::last_write_time(input.path) != input.last_mtime;
          })
        | ranges::views::transform([](auto& info) { return info.path; })  //
        | ranges::to_vector;
    deps_rebuild_info ret;
    ret.newer_inputs            = std::move(changed_files);
    ret.previous_command        = cmd.command;
    ret.previous_command_output = cmd.output;
    return ret;
}
