#include "../options.hpp"

#include <dds/build/builder.hpp>

#include <functional>

namespace dds::cli {

dds::builder create_project_builder(const options& opts);

int handle_build_error(std::function<int()>);

}  // namespace dds::cli
