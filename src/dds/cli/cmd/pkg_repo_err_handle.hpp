#pragma once

#include <functional>

namespace dds::cli::cmd {

int handle_pkg_repo_remote_errors(std::function<int()>);

}  // namespace dds::cli::cmd