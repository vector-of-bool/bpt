#include <dds/util/fs/path.hpp>

#include <catch2/catch.hpp>

#include <boost/leaf/pred.hpp>

namespace dds::testing {

const auto REPO_ROOT = fs::canonical((fs::path(__FILE__) / "../../..").lexically_normal());
const auto DATA_DIR  = REPO_ROOT / "data";

template <typename Fn>
constexpr auto leaf_handle_nofail(Fn&& fn) {
    using rtype = decltype(fn());
    if constexpr (boost::leaf::is_result_type<rtype>::value) {
        return boost::leaf::try_handle_all(  //
            fn,
            [](const boost::leaf::verbose_diagnostic_info& info) -> decltype(fn().value()) {
                FAIL("Operation failed: " << info);
                std::terminate();
            });
    } else {
        return boost::leaf::try_catch(  //
            fn,
            [](const boost::leaf::verbose_diagnostic_info& info) -> decltype(fn()) {
                FAIL("Operation failed: " << info);
                std::terminate();
            });
    }
}
#define REQUIRES_LEAF_NOFAIL(...)                                                                  \
    (::dds::testing::leaf_handle_nofail([&] { return (__VA_ARGS__); }))

}  // namespace dds::testing
