#pragma once

#ifdef __has_include
#if __has_include(<the_test_dependency_24fe5647.hpp>)
#define HAS_THE_DEP true
#endif
#endif

#ifndef HAS_THE_DEP
#define HAS_THE_DEP false
#endif

bool has_the_dep();
