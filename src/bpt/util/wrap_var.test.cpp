#include "wrap_var.hpp"

#include <string>

struct wrapped : bpt::variant_wrapper<std::string, int> {
    using variant_wrapper::variant_wrapper;
};

static_assert(std::convertible_to<int, wrapped>);
