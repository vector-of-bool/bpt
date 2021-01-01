#include <tweakable.config.hpp>
#include <tweakable.hpp>

#include <iostream>

int tweakable::get_value() { return tweakable::config::value; }
