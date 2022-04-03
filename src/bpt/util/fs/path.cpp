#include "./path.hpp"

#include <boost/leaf/result.hpp>

using namespace dds;

fs::path dds::normalize_path(path_ref p_) noexcept {
    auto p = p_.lexically_normal();
    while (!p.empty() && p.filename().empty()) {
        p = p.parent_path();
    }
    return p;
}

fs::path dds::resolve_path_weak(path_ref p) noexcept {
    return normalize_path(fs::weakly_canonical(p));
}

result<fs::path> dds::resolve_path_strong(path_ref p_) noexcept {
    std::error_code ec;
    auto            p = fs::canonical(p_, ec);
    if (ec) {
        return boost::leaf::new_error(ec, p_, e_resolve_path{p_});
    }
    return normalize_path(p);
}