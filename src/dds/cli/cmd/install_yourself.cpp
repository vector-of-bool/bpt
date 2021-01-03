#include "../options.hpp"

#include <dds/util/env.hpp>
#include <dds/util/fs.hpp>
#include <dds/util/paths.hpp>
#include <dds/util/result.hpp>
#include <dds/util/string.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <fansi/styled.hpp>
#include <fmt/ostream.h>
#include <neo/assert.hpp>
#include <neo/platform.hpp>
#include <neo/scope.hpp>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#elif __FreeBSD__
#include <sys/sysctl.h>
#elif _WIN32
#include <windows.h>
// Must be included second:
#include <wil/resource.h>
#endif

using namespace fansi::literals;

namespace dds::cli::cmd {

namespace {

fs::path current_executable() {
#if __linux__
    return fs::read_symlink("/proc/self/exe");
#elif __APPLE__
    std::uint32_t len = 0;
    _NSGetExecutablePath(nullptr, &len);
    std::string buffer;
    buffer.resize(len + 1);
    auto rc = _NSGetExecutablePath(buffer.data(), &len);
    neo_assert(invariant, rc == 0, "Unexpected error from _NSGetExecutablePath()");
    return fs::canonical(buffer);
#elif __FreeBSD__
    std::string buffer;
    int         mib[] = {CTRL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    std::size_t len   = 0;
    auto        rc    = ::sysctl(mib, 4, nullptr, &len, nullptr, 0);
    neo_assert(invariant,
               rc == 0,
               "Unexpected error from ::sysctl() while getting executable path",
               errno);
    buffer.resize(len + 1);
    rc = ::sysctl(mib, 4, buffer.data(), &len, nullptr, 0);
    neo_assert(invariant,
               rc == 0,
               "Unexpected error from ::sysctl() while getting executable path",
               errno);
    return fs::canonical(nullptr);
#elif _WIN32
    std::wstring buffer;
    while (true) {
        buffer.resize(buffer.size() + 32);
        auto reallen
            = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (reallen == buffer.size() && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            continue;
        }
        buffer.resize(reallen);
        return fs::canonical(buffer);
    }
#else
#error "No method of getting the current executable path is implemented on this system. FIXME!"
#endif
}

fs::path user_binaries_dir() noexcept {
#if _WIN32
    return dds::user_data_dir() / "bin";
#else
    return dds::user_home_dir() / ".local/bin";
#endif
}

fs::path system_binaries_dir() noexcept {
#if _WIN32
    return "C:/bin";
#else
    return "/usr/local/bin";
#endif
}

void fixup_system_path(const options&) {
#if !_WIN32
// We install into /usr/local/bin, and every nix-like system we support already has that on the
// global PATH
#else  // Windows!
#error "Not yet implemented"
#endif
}

void fixup_user_path(const options& opts) {
#if !_WIN32
    auto profile_file    = dds::user_home_dir() / ".profile";
    auto profile_content = dds::slurp_file(profile_file);
    if (dds::contains(profile_content, "$HOME/.local/bin")) {
        // We'll assume that this is properly loading .local/bin for .profile
        dds_log(info, ".br.cyan[{}] is okay"_styled, profile_file);
    } else {
        // Let's add it
        profile_content
            += ("\n# This entry was added by 'dds install-yourself' for the user-local "
                "binaries\nPATH=$HOME/bin:$HOME/.local/bin:$PATH\n");
        if (opts.dry_run) {
            dds_log(info,
                    "Would update .br.cyan[{}] to have ~/.local/bin on $PATH"_styled,
                    profile_file);
        } else {
            dds_log(info,
                    "Updating .br.cyan[{}] with a user-local binaries PATH entry"_styled,
                    profile_file);
            auto tmp_file = profile_file;
            tmp_file += ".tmp";
            auto bak_file = profile_file;
            bak_file += ".bak";
            // Move .profile back into place if we abore for any reason
            neo_defer {
                if (!fs::exists(profile_file)) {
                    safe_rename(bak_file, profile_file);
                }
            };
            // Write the temporary version
            dds::write_file(tmp_file, profile_content).value();
            // Make a backup
            safe_rename(profile_file, bak_file);
            // Move the tmp over the final location
            safe_rename(tmp_file, profile_file);
            // Okay!
            dds_log(info,
                    ".br.green[{}] was updated. Prior contents are safe in .br.cyan[{}]"_styled,
                    profile_file,
                    bak_file);
        }
    }

    auto fish_config = dds::user_config_dir() / "fish/config.fish";
    if (fs::exists(fish_config)) {
        auto fish_config_content = slurp_file(fish_config);
        if (dds::contains(fish_config_content, "$HOME/.local/bin")) {
            // Assume that this is up-to-date
            dds_log(info, "Fish configuration in .br.cyan[{}] is okay"_styled, fish_config);
        } else {
            dds_log(
                info,
                "Updating Fish shell configuration .br.cyan[{}] with user-local binaries PATH entry"_styled,
                fish_config);
            fish_config_content
                += ("\n# This line was added by 'dds install-yourself' to add the usre-local "
                    "binaries directory to $PATH\nset -x PATH $PATH \"$HOME/.local/bin\"\n");
            auto tmp_file = fish_config;
            auto bak_file = fish_config;
            tmp_file += ".tmp";
            bak_file += ".bak";
            neo_defer {
                if (!fs::exists(fish_config)) {
                    safe_rename(bak_file, fish_config);
                }
            };
            // Write the temporary version
            dds::write_file(tmp_file, fish_config_content).value();
            // Make a backup
            safe_rename(fish_config, bak_file);
            // Move the temp over the destination
            safe_rename(tmp_file, fish_config);
            // Okay!
            dds_log(info,
                    ".br.green[{}] was updated. Prior contents are safe in .br.cyan[{}]"_styled,
                    fish_config,
                    bak_file);
        }
    }
#else  // _WIN32
#endif
}

void fixup_path(const options& opts) {
    if (opts.install_yourself.where == opts.install_yourself.system) {
        fixup_system_path(opts);
    } else {
        fixup_user_path(opts);
    }
}

int _install_yourself(const options& opts) {
    auto self_exe = current_executable();

    auto dest_dir = opts.install_yourself.where == opts.install_yourself.user
        ? user_binaries_dir()
        : system_binaries_dir();

    auto dest_path = dest_dir / "dds";
    if constexpr (neo::os_is_windows) {
        dest_path += ".exe";
    }

    if (fs::canonical(dest_path) == fs::canonical(self_exe)) {
        dds_log(error, "We cannot install over our own executable (.br.red[{}])"_styled, self_exe);
        return 1;
    }

    if (!fs::is_directory(dest_dir)) {
        if (opts.dry_run) {
            dds_log(info, "Would create directory .br.cyan[{}]"_styled, dest_dir);
        } else {
            dds_log(info, "Creating directory .br.cyan[{}]"_styled, dest_dir);
            fs::create_directories(dest_dir);
        }
    }

    if (opts.dry_run) {
        if (fs::is_symlink(dest_path)) {
            dds_log(info, "Would remove symlink .br.cyan[{}]"_styled, dest_path);
        }
        if (fs::exists(dest_path) && !fs::is_symlink(dest_path)) {
            if (opts.install_yourself.symlink) {
                dds_log(
                    info,
                    "Would overwrite .br.yellow[{0}] with a symlink .br.green[{0}] -> .br.cyan[{1}]"_styled,
                    dest_path,
                    self_exe);
            } else {
                dds_log(info,
                        "Would overwrite .br.yellow[{}] with .br.cyan[{}]"_styled,
                        dest_path,
                        self_exe);
            }
        } else {
            if (opts.install_yourself.symlink) {
                dds_log(info,
                        "Would create a symlink .br.green[{}] -> .br.cyan[{}]"_styled,
                        dest_path,
                        self_exe);
            } else {
                dds_log(info,
                        "Would install .br.cyan[{}] to .br.yellow[{}]"_styled,
                        self_exe,
                        dest_path);
            }
        }
    } else {
        if (fs::is_symlink(dest_path)) {
            dds_log(info, "Removing old symlink file .br.cyan[{}]"_styled, dest_path);
            dds::remove_file(dest_path).value();
        }
        if (opts.install_yourself.symlink) {
            if (fs::exists(dest_path)) {
                dds_log(info, "Removing previous file .br.cyan[{}]"_styled, dest_path);
                dds::remove_file(dest_path).value();
            }
            dds_log(info,
                    "Creating symbolic link .br.green[{}] -> .br.cyan[{}]"_styled,
                    dest_path,
                    self_exe);
            dds::create_symlink(self_exe, dest_path).value();
        } else {
            dds_log(info, "Installing .br.cyan[{}] to .br.green[{}]"_styled, self_exe, dest_path);
            dds::copy_file(self_exe, dest_path, fs::copy_options::overwrite_existing).value();
        }
    }

    if (opts.install_yourself.fixup_path_env) {
        fixup_path(opts);
    }

    if (!opts.dry_run) {
        dds_log(info, "Success!");
    }
    return 0;
}

}  // namespace

int install_yourself(const options& opts) {
    return boost::leaf::try_catch(
        [&] {
            try {
                return _install_yourself(opts);
            } catch (...) {
                capture_exception();
            }
        },
        [](std::error_code ec, e_copy_file copy) {
            dds_log(error,
                    "Failed to copy file .br.cyan[{}] to .br.yellow[{}]: .bold.red[{}]"_styled,
                    copy.source,
                    copy.dest,
                    ec.message());
            return 1;
        },
        [](std::error_code ec, e_remove_file file) {
            dds_log(error,
                    "Failed to delete file .br.yellow[{}]: .bold.red[{}]"_styled,
                    file.value,
                    ec.message());
            return 1;
        },
        [](std::error_code ec, e_symlink oper) {
            dds_log(
                error,
                "Failed to create symlink from .br.yellow[{}] to .br.cyan[{}]: .bold.red[{}]"_styled,
                oper.symlink,
                oper.target,
                ec.message());
            return 1;
        },
        [](e_system_error_exc e) {
            dds_log(error, "Failure while installing: {}", e.message);
            return 1;
        });
    return 0;
}

}  // namespace dds::cli::cmd
