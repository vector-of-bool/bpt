struct bad_src {};

#include "./bad_src.hpp"

bad_src bad_function() { return bad_src{}; }
