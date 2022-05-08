#include "./shutil.hpp"

#include "./path.hpp"

#include <bpt/error/on_error.hpp>
#include <bpt/error/result.hpp>
#include <bpt/error/try_catch.hpp>
#include <bpt/temp.hpp>
#include <bpt/util/log.hpp>

#include <neo/ufmt.hpp>

#include <system_error>

using namespace bpt;

result<void> bpt::ensure_absent(path_ref path) noexcept {
    BPT_E_SCOPE(e_remove_file{path});
    std::error_code ec;
    bpt_log(trace, "Recursive ensure-absent [{}]", path.string());
    fs::remove_all(path, ec);
    if (ec) {
        const bool is_enoent = ec == std::errc::no_such_file_or_directory;
        bpt_log(trace,
                "  Ensure-absent error while removing [{}]: {}{}",
                path.string(),
                ec.message(),
                is_enoent ? " (Ignoring this error)" : "");
        if (not is_enoent) {
            return new_error(ec);
        }
    }
    return {};
}

result<void> bpt::remove_file(path_ref path) noexcept {
    BPT_E_SCOPE(e_remove_file{path});
    std::error_code ec;
    fs::remove(path, ec);
    if (ec) {
        return new_error(ec);
    }
    return {};
}

result<void> bpt::move_file(path_ref source, path_ref dest) {
    std::error_code ec;
    BPT_E_SCOPE(e_move_file{source, dest});
    BPT_E_SCOPE(ec);

    fs::rename(source, dest, ec);
    if (!ec) {
        return {};
    }

    if (ec != std::errc::cross_device_link && ec != std::errc::permission_denied) {
        BOOST_LEAF_NEW_ERROR();
    }

    auto tmp = bpt_leaf_try_some { return bpt::temporary_dir::create_in(dest.parent_path()); }
    bpt_leaf_catch(const std::system_error& exc) { return BOOST_LEAF_NEW_ERROR(exc, exc.code()); };
    BOOST_LEAF_CHECK(tmp);

    fs::copy(source, tmp->path(), fs::copy_options::recursive, ec);
    if (ec) {
        return BOOST_LEAF_NEW_ERROR();
    }
    fs::rename(tmp->path(), dest, ec);
    if (ec) {
        return BOOST_LEAF_NEW_ERROR();
    }
    fs::remove_all(source, ec);
    // Drop 'ec'
    return {};
}

result<void> bpt::copy_file(path_ref source, path_ref dest, fs::copy_options opts) noexcept {
    std::error_code ec;
    BPT_E_SCOPE(e_copy_file{source, dest});
    BPT_E_SCOPE(ec);
    opts &= ~fs::copy_options::recursive;
    fs::copy_file(source, dest, opts, ec);
    if (ec) {
        return BOOST_LEAF_NEW_ERROR();
    }
    return {};
}

result<void> bpt::create_symlink(path_ref target, path_ref symlink) noexcept {
    std::error_code ec;
    BPT_E_SCOPE(e_symlink{symlink, target});
    BPT_E_SCOPE(ec);
    /// XXX: 'target' might not refer to an existing file, or might be a relative path from the
    /// symlink dest dir.
    if (fs::is_directory(target)) {
        fs::create_directory_symlink(target, symlink, ec);
    } else {
        fs::create_symlink(target, symlink, ec);
    }
    if (ec) {
        return BOOST_LEAF_NEW_ERROR();
    }
    return {};
}

static result<void> copy_tree_1(path_ref source, path_ref dest, fs::copy_options opts) noexcept {
    if (!fs::is_directory(source)) {
        return BOOST_LEAF_NEW_ERROR(std::make_error_code(std::errc::not_a_directory));
    }
    if (!fs::is_directory(dest)) {
        std::error_code ec;
        fs::create_directories(dest, ec);
        if (ec) {
            return BOOST_LEAF_NEW_ERROR(ec);
        }
    }
    for (fs::directory_entry entry : fs::directory_iterator{source}) {
        auto subdest = dest / entry.path().filename();
        if (entry.is_directory()) {
            BOOST_LEAF_CHECK(copy_tree_1(entry.path(), subdest, opts));
        } else {
            BOOST_LEAF_CHECK(bpt::copy_file(entry.path(), subdest, opts));
        }
    }
    return {};
}

result<void> bpt::copy_tree(path_ref source, path_ref dest, fs::copy_options opts) noexcept {
    BPT_E_SCOPE(e_copy_tree{source, dest});
    BOOST_LEAF_AUTO(src, resolve_path_strong(source));
    auto dst = resolve_path_weak(dest);
    return copy_tree_1(src, dst, opts);
}
