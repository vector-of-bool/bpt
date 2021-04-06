bool dependent_has_the_dep() {
#if __has_include(<the_test_dependency_24fe5647.hpp>)
    return true;
#else
    return false;
#endif
}

bool dependent_has_direct_dep() {
#if __has_include(<the_test_lib_d1df7125.hpp>)
    return true;
#else
    return false;
#endif
}
