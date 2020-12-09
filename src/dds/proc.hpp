#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dds {

bool needs_quoting(std::string_view);

std::string quote_argument(std::string_view);

template <typename Container>
std::string quote_command(const Container& c) {
    std::string acc;
    for (const auto& arg : c) {
        acc += quote_argument(arg) + " ";
    }
    if (!acc.empty()) {
        acc.pop_back();
    }
    return acc;
}

struct proc_result {
    int         signal    = 0;
    int         retc      = 0;
    bool        timed_out = false;
    std::string output;

    bool okay() const noexcept { return retc == 0 && signal == 0; }
};

struct proc_options {
    std::vector<std::string> command;

    std::optional<std::filesystem::path> cwd = std::nullopt;

    /**
     * Timeout for the subprocess, in milliseconds. If zero, will wait forever
     */
    std::optional<std::chrono::milliseconds> timeout = std::nullopt;
};

proc_result run_proc(const proc_options& opts);

inline proc_result run_proc(std::vector<std::string> args) {
    return run_proc(proc_options{.command = std::move(args)});
}

}  // namespace dds
