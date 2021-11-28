#include "../options.hpp"

#include <dds/util/env.hpp>
#include <dds/util/fs/io.hpp>
#include <dds/util/fs/shutil.hpp>
#include <dds/util/paths.hpp>
#include <dds/util/result.hpp>
#include <dds/util/string.hpp>

#include <boost/leaf/pred.hpp>
#include <fansi/styled.hpp>
#include <neo/assert.hpp>
#include <neo/platform.hpp>
#include <neo/scope.hpp>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#elif __FreeBSD__
#include <sys/types.h>
// <sys/types.h> must come first
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
    int         mib[]  = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    std::size_t len    = 0;
    auto        rc     = ::sysctl(mib, 4, nullptr, &len, nullptr, 0);
    auto        errno_ = errno;
    neo_assert(invariant,
               rc == 0,
               "Unexpected error from ::sysctl() while getting executable path",
               errno_);
    buffer.resize(len + 1);
    rc     = ::sysctl(mib, 4, buffer.data(), &len, nullptr, 0);
    errno_ = errno;
    neo_assert(invariant,
               rc == 0,
               "Unexpected error from ::sysctl() while getting executable path",
               errno_);
    return fs::canonical(buffer);
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

#if _WIN32
void fixup_path_env(const options& opts, const wil::unique_hkey& env_hkey, fs::path want_path) {
    DWORD len = 0;
    // Get the length
    auto err = ::RegGetValueW(env_hkey.get(),
                              nullptr,
                              L"PATH",
                              RRF_RT_REG_EXPAND_SZ | RRF_RT_REG_SZ | RRF_NOEXPAND,
                              nullptr,
                              nullptr,
                              &len);
    if (err != ERROR_SUCCESS) {
        throw std::system_error(std::error_code(err, std::system_category()),
                                "Failed to access PATH environment variable [1]");
    }
    // Now get the value
    std::wstring buffer;
    buffer.resize(len / 2);
    err = ::RegGetValueW(env_hkey.get(),
                         nullptr,
                         L"PATH",
                         RRF_RT_REG_EXPAND_SZ | RRF_RT_REG_SZ | RRF_NOEXPAND,
                         nullptr,
                         buffer.data(),
                         &len);
    if (err != ERROR_SUCCESS) {
        throw std::system_error(std::error_code(err, std::system_category()),
                                "Failed to access PATH environment variable [2]");
    }
    // Strip null-term
    buffer.resize(len);
    while (!buffer.empty() && buffer.back() == 0) {
        buffer.pop_back();
    }
    // Check if we need to append the user-local binaries dir to the path
    const auto want_entry   = fs::path(want_path).make_preferred().lexically_normal();
    const auto path_env_str = fs::path(buffer).string();
    auto       path_elems   = split_view(path_env_str, ";");
    const bool any_match    = std::any_of(path_elems.cbegin(), path_elems.cend(), [&](auto view) {
        auto existing = fs::weakly_canonical(view).make_preferred().lexically_normal();
        dds_log(trace, "Existing PATH entry: '{}'", existing.string());
        return existing.native() == want_entry.native();
    });
    if (any_match) {
        dds_log(info, "PATH is up-to-date");
        return;
    }
    if (opts.dry_run) {
        dds_log(info, "The PATH environment variable would be modified.");
        return;
    }
    // It's not there. Add it now.
    auto want_str = want_entry.string();
    path_elems.insert(path_elems.begin(), want_str);
    auto joined = joinstr(";", path_elems);
    buffer      = fs::path(joined).native();
    // Put the new PATH entry back into the environment
    err = ::RegSetValueExW(env_hkey.get(),
                           L"Path",
                           0,
                           REG_EXPAND_SZ,
                           reinterpret_cast<const BYTE*>(buffer.data()),
                           (buffer.size() + 1) * 2);
    if (err != ERROR_SUCCESS) {
        throw std::system_error(std::error_code(err, std::system_category()),
                                "Failed to modify PATH environment variable");
    }
    dds_log(
        info,
        "The directory [.br.cyan[{}]] has been added to your PATH environment variables."_styled,
        want_path.string());
    dds_log(
        info,
        ".bold.cyan[NOTE:] You may need to restart running applications to see this change!"_styled);
}
#endif

void fixup_system_path(const options& opts [[maybe_unused]]) {
#if !_WIN32
// We install into /usr/local/bin, and every nix-like system we support already has that on the
// global PATH
#else  // Windows!
    wil::unique_hkey env_hkey;
    auto             err = ::RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                               L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
                               0,
                               KEY_WRITE | KEY_READ,
                               &env_hkey);
    if (err != ERROR_SUCCESS) {
        throw std::system_error(std::error_code(err, std::system_category()),
                                "Failed to open user-local environment variables registry "
                                "entry");
    }
    fixup_path_env(opts, env_hkey, "C:/bin");
#endif
}

void fixup_user_path(const options& opts) {
#if !_WIN32
    auto profile_file    = dds::user_home_dir() / ".profile";
    auto profile_content = dds::read_file(profile_file);
    if (dds::contains(profile_content, "$HOME/.local/bin")) {
        // We'll assume that this is properly loading .local/bin for .profile
        dds_log(info, "[.br.cyan[{}]] is okay"_styled, profile_file.string());
    } else if (opts.dry_run) {
        dds_log(info,
                "Would update [.br.cyan[{}]] to have ~/.local/bin on $PATH"_styled,
                profile_file.string());
    } else {
        // Let's add it
        profile_content
            += ("\n# This entry was added by 'dds install-yourself' for the user-local "
                "binaries path\nPATH=$HOME/bin:$HOME/.local/bin:$PATH\n");
        dds_log(info,
                "Updating [.br.cyan[{}]] with a user-local binaries PATH entry"_styled,
                profile_file.string());
        auto tmp_file = profile_file;
        tmp_file += ".tmp";
        auto bak_file = profile_file;
        bak_file += ".bak";
        // Move .profile back into place if we abore for any reason
        neo_defer {
            if (!fs::exists(profile_file)) {
                move_file(bak_file, profile_file).value();
            }
        };
        // Write the temporary version
        dds::write_file(tmp_file, profile_content);
        // Make a backup
        move_file(profile_file, bak_file).value();
        // Move the tmp over the final location
        move_file(tmp_file, profile_file).value();
        // Okay!
        dds_log(info,
                "[.br.green[{}]] was updated. Prior contents are safe in [.br.cyan[{}]]"_styled,
                profile_file.string(),
                bak_file.string());
        dds_log(
            info,
            ".bold.cyan[NOTE:] Running applications may need to be restarted to see this change"_styled);
    }

    auto fish_config = dds::user_config_dir() / "fish/config.fish";
    if (fs::exists(fish_config)) {
        auto fish_config_content = dds::read_file(fish_config);
        if (dds::contains(fish_config_content, "$HOME/.local/bin")) {
            // Assume that this is up-to-date
            dds_log(info,
                    "Fish configuration in [.br.cyan[{}]] is okay"_styled,
                    fish_config.string());
        } else if (opts.dry_run) {
            dds_log(info,
                    "Would update [.br.cyan[{}]] to have ~/.local/bin on $PATH"_styled,
                    fish_config.string());
        } else {
            dds_log(
                info,
                "Updating Fish shell configuration [.br.cyan[{}]] with user-local binaries PATH entry"_styled,
                fish_config.string());
            fish_config_content
                += ("\n# This line was added by 'dds install-yourself' to add the user-local "
                    "binaries directory to $PATH\nset -x PATH $PATH \"$HOME/.local/bin\"\n");
            auto tmp_file = fish_config;
            auto bak_file = fish_config;
            tmp_file += ".tmp";
            bak_file += ".bak";
            neo_defer {
                if (!fs::exists(fish_config)) {
                    move_file(bak_file, fish_config).value();
                }
            };
            // Write the temporary version
            dds::write_file(tmp_file, fish_config_content);
            // Make a backup
            move_file(fish_config, bak_file).value();
            // Move the temp over the destination
            move_file(tmp_file, fish_config).value();
            // Okay!
            dds_log(info,
                    "[.br.green[{}]] was updated. Prior contents are safe in [.br.cyan[{}]]"_styled,
                    fish_config.string(),
                    bak_file.string());
            dds_log(
                info,
                ".bold.cyan[NOTE:] Running Fish shells will need to be restartred to see this change"_styled);
        }
    }
#else  // _WIN32
    wil::unique_hkey env_hkey;
    auto             err
        = ::RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_WRITE | KEY_READ, &env_hkey);
    if (err != ERROR_SUCCESS) {
        throw std::system_error(std::error_code(err, std::system_category()),
                                "Failed to open user-local environment variables registry "
                                "entry");
    }
    fixup_path_env(opts, env_hkey, "%LocalAppData%/bin");
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

    if (fs::absolute(dest_path).lexically_normal() == fs::canonical(self_exe)) {
        dds_log(error,
                "We cannot install over our own executable (.br.red[{}])"_styled,
                self_exe.string());
        return 1;
    }

    if (!fs::is_directory(dest_dir)) {
        if (opts.dry_run) {
            dds_log(info, "Would create directory [.br.cyan[{}]]"_styled, dest_dir.string());
        } else {
            dds_log(info, "Creating directory [.br.cyan[{}]]"_styled, dest_dir.string());
            fs::create_directories(dest_dir);
        }
    }

    if (opts.dry_run) {
        if (fs::is_symlink(dest_path)) {
            dds_log(info, "Would remove symlink [.br.cyan[{}]]"_styled, dest_path.string());
        }
        if (fs::exists(dest_path) && !fs::is_symlink(dest_path)) {
            if (opts.install_yourself.symlink) {
                dds_log(
                    info,
                    "Would overwrite .br.yellow[{0}] with a symlink .br.green[{0}] -> .br.cyan[{1}]"_styled,
                    dest_path.string(),
                    self_exe.string());
            } else {
                dds_log(info,
                        "Would overwrite .br.yellow[{}] with [.br.cyan[{}]]"_styled,
                        dest_path.string(),
                        self_exe.string());
            }
        } else {
            if (opts.install_yourself.symlink) {
                dds_log(info,
                        "Would create a symlink [.br.green[{}]] -> [.br.cyan[{}]]"_styled,
                        dest_path.string(),
                        self_exe.string());
            } else {
                dds_log(info,
                        "Would install [.br.cyan[{}]] to .br.yellow[{}]"_styled,
                        self_exe.string(),
                        dest_path.string());
            }
        }
    } else {
        if (fs::is_symlink(dest_path)) {
            dds_log(info, "Removing old symlink file [.br.cyan[{}]]"_styled, dest_path.string());
            dds::remove_file(dest_path).value();
        }
        if (opts.install_yourself.symlink) {
            if (fs::exists(dest_path)) {
                dds_log(info, "Removing previous file [.br.cyan[{}]]"_styled, dest_path.string());
                dds::remove_file(dest_path).value();
            }
            dds_log(info,
                    "Creating symbolic link [.br.green[{}]] -> [.br.cyan[{}]]"_styled,
                    dest_path.string(),
                    self_exe.string());
            dds::create_symlink(self_exe, dest_path).value();
        } else {
            dds_log(info,
                    "Installing [.br.cyan[{}]] to [.br.green[{}]]"_styled,
                    self_exe.string(),
                    dest_path.string());
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
        [&] { return _install_yourself(opts); },
        [](std::error_code ec, e_copy_file copy) {
            dds_log(error,
                    "Failed to copy file [.br.cyan[{}]] to .br.yellow[{}]: .bold.red[{}]"_styled,
                    copy.source.string(),
                    copy.dest.string(),
                    ec.message());
            return 1;
        },
        [](std::error_code ec, e_remove_file file) {
            dds_log(error,
                    "Failed to delete file .br.yellow[{}]: .bold.red[{}]"_styled,
                    file.value.string(),
                    ec.message());
            return 1;
        },
        [](std::error_code ec, e_symlink oper) {
            dds_log(
                error,
                "Failed to create symlink from .br.yellow[{}] to [.br.cyan[{}]]: .bold.red[{}]"_styled,
                oper.symlink.string(),
                oper.target.string(),
                ec.message());
            return 1;
        },
        [](const std::system_error& e) {
            dds_log(error, "Failure while installing: {}", e.code().message());
            return 1;
        });
    return 0;
}

}  // namespace dds::cli::cmd
