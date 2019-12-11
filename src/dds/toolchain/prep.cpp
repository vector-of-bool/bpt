#include "./prep.hpp"

#include <dds/toolchain/toolchain.hpp>

using namespace dds;

toolchain toolchain_prep::realize() const { return toolchain::realize(*this); }
