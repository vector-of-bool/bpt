#include <dds/repoman/repoman.hpp>

#include <dds/pkg/listing.hpp>
#include <dds/temp.hpp>

#include <neo/sqlite3/error.hpp>

#include <catch2/catch.hpp>

namespace {

const auto THIS_FILE = dds::fs::canonical(__FILE__);
const auto THIS_DIR  = THIS_FILE.parent_path();
const auto REPO_ROOT = (THIS_DIR / "../../../").lexically_normal();
const auto DATA_DIR  = REPO_ROOT / "data";

struct tmp_repo {
    dds::temporary_dir tempdir = dds::temporary_dir::create();
    dds::repo_manager  repo    = dds::repo_manager::create(tempdir.path(), "test-repo");
};

}  // namespace
