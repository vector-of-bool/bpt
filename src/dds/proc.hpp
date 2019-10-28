#pragma once

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
    int         signal = 0;
    int         retc   = 0;
    std::string output;

    bool okay() const noexcept { return retc == 0 && signal == 0; }
};

proc_result run_proc(const std::vector<std::string>& args);

}  // namespace dds
