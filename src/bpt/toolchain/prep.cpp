#include "./prep.hpp"

#include <bpt/toolchain/toolchain.hpp>
#include <bpt/util/env.hpp>
#include <bpt/util/siphash.hpp>
#include <bpt/util/string.hpp>

#include <neo/utility.hpp>
#include <nlohmann/json.hpp>
#include <range/v3/range/conversion.hpp>

#include <ranges>

using namespace dds;
using json = nlohmann::json;

toolchain toolchain_prep::realize() const { return toolchain::realize(*this); }

std::uint64_t toolchain_prep::compute_hash() const noexcept {
    auto should_prune_flag = [](std::string_view v) {
        bool prune = v == neo::oper::any_of("-fdiagnostics-color", "/nologo");
        if (prune) {
            return prune;
        } else if (v.starts_with("-fconcept-diagnostics-depth=")) {
            return true;
        } else {
            return false;
        }
    };
    auto prune_flags = [&](auto&& arr) {
        return arr  //
            | std::views::filter([&](auto&& s) { return !should_prune_flag(s); })
            | ranges::v3::to_vector;
    };

    // Only convert the details relevant to the ABI
    auto root = json::object({
        {"c_compile", json(prune_flags(c_compile))},
        {"cxx_compile", json(prune_flags(cxx_compile))},
    });
    auto env  = json::object();
    for (auto& varname : consider_envs) {
        auto val = dds::getenv(varname);
        if (val) {
            env[varname] = *val;
        }
    }
    root["env"] = env;
    // Make a very normalized document
    std::string s = root.dump();
    return dds::siphash64(42, 1729, neo::const_buffer(s)).digest();
}
