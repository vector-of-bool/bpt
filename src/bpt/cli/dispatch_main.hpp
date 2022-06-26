#pragma once

namespace bpt::cli {

struct options;

int dispatch_main(const options&) noexcept;

}  // namespace bpt::cli