#include "the_test_lib_d1df7125.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>

int main() {
    assert(HAS_THE_DEP);
    assert(!::has_the_dep());
}
