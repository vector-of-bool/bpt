#pragma once

#include <exception>
#include <stdexcept>
#include <string>

namespace debate {

class argument;
class argument_parser;
class subparser;

struct help_request : std::exception {};

struct invalid_arguments : std::runtime_error {
    using runtime_error::runtime_error;
};

struct unrecognized_argument : invalid_arguments {
    using invalid_arguments::invalid_arguments;
};

struct missing_required : invalid_arguments {
    using invalid_arguments::invalid_arguments;
};

struct invalid_repitition : invalid_arguments {
    using invalid_arguments::invalid_arguments;
};

struct e_argument {
    const debate::argument& argument;
};

struct e_argument_parser {
    const debate::argument_parser& parser;
};

struct e_invalid_arg_value {
    std::string given;
};

struct e_wrong_val_num {
    int n_given;
};

struct e_arg_spelling {
    std::string spelling;
};

struct e_did_you_mean {
    std::string candidate;
};

}  // namespace debate
