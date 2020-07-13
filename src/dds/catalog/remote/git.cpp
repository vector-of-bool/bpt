#include "./git.hpp"

#include <dds/error/errors.hpp>
#include <dds/proc.hpp>

void dds::git_remote_listing::clone(dds::path_ref dest) const {
    fs::remove_all(dest);
    using namespace std::literals;
    auto command = {"git"s, "clone"s, "--depth=1"s, "--branch"s, ref, url, dest.generic_string()};
    auto git_res = run_proc(command);
    if (!git_res.okay()) {
        throw_external_error<errc::git_clone_failure>(
            "Git clone operation failed [Git command: {}] [Exitted {}]:\n{}",
            quote_command(command),
            git_res.retc,
            git_res.output);
    }
}
