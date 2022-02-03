#include "../options.hpp"

#include <dds/crs/repo.hpp>
#include <dds/error/marker.hpp>
#include <dds/error/try_catch.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <iostream>

namespace dds::cli::cmd {

int repo_ls(const options& opts) {
    auto repo = dds::crs::repository::open_existing(opts.repo.repo_dir);
    for (auto pkg : repo.all_packages()) {
        fmt::print(std::cout, pkg.id.to_string());
        std::cout << '\n';
    }
    return 0;
}

}  // namespace dds::cli::cmd
