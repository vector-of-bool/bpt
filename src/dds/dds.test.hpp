#include <dds/util/fs.hpp>

#include <catch2/catch.hpp>

namespace dds::testing {

const auto REPO_ROOT = fs::canonical((fs::path(__FILE__) / "../../..").lexically_normal());
const auto DATA_DIR  = REPO_ROOT / "data";

}  // namespace dds::testing
