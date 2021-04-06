#include "the_test_lib_d1df7125.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main() {
    assert(!::has_the_dep() && "Serious error; the_test_lib is improperly set up");
    assert(!HAS_THE_DEP && "Leaked test_depends to final dependent");
}
