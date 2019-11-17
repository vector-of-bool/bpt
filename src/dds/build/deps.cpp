#include "./deps.hpp"

#include <dds/db/database.hpp>
#include <dds/proc.hpp>
#include <dds/util/shlex.hpp>
#include <dds/util/string.hpp>

#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

using namespace dds;

deps_info dds::parse_mkfile_deps_file(path_ref where) {
    auto content = slurp_file(where);
    return parse_mkfile_deps_str(content);
}

deps_info dds::parse_mkfile_deps_str(std::string_view str) {
    deps_info ret;

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
    auto lines = split_view(output, "\n");
    std::string cleaned_output;
    deps_info   deps;
    for (auto line : lines) {
        line = trim_view(line);
        if (!starts_with(line, leader)) {
            cleaned_output += std::string(line);
            cleaned_output.push_back('\n');
            continue;
        }
        auto remaining = trim_view(line.substr(leader.size()));
        deps.inputs.emplace_back(fs::weakly_canonical(remaining));
    }
    if (!cleaned_output.empty()) {
        // Remove the extra newline at the back
        cleaned_output.pop_back();
    }
    return {deps, cleaned_output};
}

void dds::update_deps_info(database& db, const deps_info& deps) {
    db.store_mtime(deps.output, fs::last_write_time(deps.output));
    db.store_file_command(deps.output, {deps.command, deps.command_output});
    db.forget_inputs_of(deps.output);
    for (auto&& inp : deps.inputs) {
        db.store_mtime(inp, fs::last_write_time(inp));
        db.record_dep(inp, deps.output);
    }
}

deps_rebuild_info dds::get_rebuild_info(database& db, path_ref output_path) {
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
        | ranges::views::filter([](const seen_file_info& input) {
              return fs::last_write_time(input.path) != input.last_mtime;
          })
        | ranges::views::transform([](auto& info) { return info.path; })  //
        | ranges::to_vector;
    deps_rebuild_info ret;
    ret.newer_inputs            = std::move(changed_files);
    ret.previous_command        = cmd.command;
    ret.previous_command_output = cmd.output;
    return ret;
}
