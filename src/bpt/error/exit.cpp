#include "./exit.hpp"

#include <boost/leaf/error.hpp>
#include <boost/leaf/exception.hpp>

void bpt::throw_system_exit(int rc) {
    BOOST_LEAF_THROW_EXCEPTION(boost::leaf::current_error(), e_exit{rc});
}
