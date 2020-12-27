#pragma once

namespace dds::cli {

struct options;

int dispatch_main(const options&) noexcept;

}  // namespace dds