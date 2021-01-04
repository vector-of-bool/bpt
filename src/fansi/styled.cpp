#include "./styled.hpp"

#include "./style.hpp"
#include "./writer.hpp"

#include <magic_enum.hpp>
#include <neo/buffer_algorithm/concat.hpp>
#include <neo/event.hpp>
#include <neo/string_io.hpp>
#include <neo/ufmt.hpp>
#include <neo/utility.hpp>

#include <cctype>
#include <charconv>
#include <map>
#include <vector>

#if NEO_OS_IS_WINDOWS
#include <windows.h>

bool fansi::detect_should_style() noexcept {
    auto stdio_console = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdio_console == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD mode = 0;
    if (!::GetConsoleMode(stdio_console, &mode)) {
        // Failed to get the mode
        return false;
    }
    return (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
#else
#include <unistd.h>
bool fansi::detect_should_style() noexcept { return ::isatty(STDOUT_FILENO); }
#endif

using namespace fansi;
using namespace neo::buffer_literals;

namespace {

const auto ANSI_CSI = "\x1b["_buf;
// const auto ANSI_RESET   = "0m"_buf;
// const auto ANSI_BOLD    = "1m"_buf;
// const auto ANSI_RED     = "32m"_buf;
// const auto ANSI_GREEN   = "32m"_buf;
// const auto ANSI_YELLOW  = "33m"_buf;
// const auto ANSI_BLUE    = "34m"_buf;
// const auto ANSI_MAGENTA = "35m"_buf;
// const auto ANSI_CYAN    = "36m"_buf;
// const auto ANSI_WHITE   = "37m"_buf;
// const auto ANSI_GRAY    = "90m"_buf;

constexpr text_style default_style{};

struct text_styler {
    std::string_view input;
    should_style     should;
    text_writer      out{};

    std::string_view::iterator s_iter = input.cbegin(), s_place = s_iter, s_stop = input.cend();

    bool do_style = (should == should_style::force)
        ? true
        : (should == should_style::never ? false : detect_should_style());

    std::vector<text_style> _style_stack = {default_style};

    std::string_view slice(std::string_view::iterator it,
                           std::string_view::iterator st) const noexcept {
        return input.substr(it - input.cbegin(), st - it);
    }
    std::string_view pending() const noexcept { return slice(s_place, s_iter); }
    std::string_view remaining() const noexcept { return slice(s_place, input.cend()); }

    std::string render() noexcept {
        while (s_iter != s_stop) {
            if (*s_iter == '`') {
                out.write(pending());
                ++s_iter;
                if (s_iter == s_stop) {
                    neo::emit(ev_warning{"String ends with incomplete escape sequence"});
                } else {
                    out.putc(*s_iter);
                }
                ++s_iter;
                s_place = s_iter;
            } else if (*s_iter == '.') {
                out.write(pending());
                s_place = s_iter;
                ++s_iter;
                if (s_iter == s_stop || !std::isalpha(*s_iter)) {
                    // Just keep going
                    continue;
                }
                s_place = s_iter;
                _push_style();
            } else if (*s_iter == ']' && _style_stack.size() > 1) {
                out.write(pending());
                s_place = ++s_iter;
                _pop_style();
            } else {
                // Just keep scanning
                ++s_iter;
            }
        }
        out.write(pending());
        return out.take_string();
    }

    void _push_style() noexcept {
        _read_style();
        neo_assert(expects,
                   *s_iter == '[',
                   "Style sequence should be followed by an opening square brackent");
        if (do_style) {
            out.put_style(_style_stack.back());
        }
        s_place = ++s_iter;
    }

    void _read_style() noexcept {
        auto& style = _style_stack.emplace_back(_style_stack.back());
        while (s_iter != s_stop) {
            if (*s_iter == neo::oper::any_of('[', '.')) {
                auto cls = pending();
                s_place  = s_iter;
                _apply_class(style, cls);
                if (*s_iter == '[') {
                    return;
                }
                s_place = ++s_iter;
            }
            ++s_iter;
        }
    }

    void _apply_class(text_style& style, std::string_view cls) const noexcept {
        auto color = magic_enum::enum_cast<std_color>(cls);
        if (color) {
            style.fg_color = *color;
        }
#define CASE(Name)                                                                                 \
    else if (cls == #Name) {                                                                       \
        style.Name = true;                                                                         \
    }
        CASE(bold)
        CASE(faint)
        CASE(italic)
        CASE(underline)
        CASE(reverse)
        CASE(strike)
#undef CASE
        else if (cls == "br") {
            style.bright = true;
        }
        else {
            neo_assert(expects, false, "Invalid text style class in input string", cls);
        }
    }

    void _pop_style() noexcept {
        neo_assert(expects,
                   _style_stack.size() > 1,
                   "Unbalanced style: Extra closing square brackets");
        _style_stack.pop_back();
        out.put_style(_style_stack.back());
    }
};  // namespace

}  // namespace

std::string fansi::stylize(std::string_view str, fansi::should_style should) {
    neo_assertion_breadcrumbs("Rendering text style string", str);
    return text_styler{str, should}.render();
}

const std::string& detail::cached_rendering(const char* ptr) noexcept {
    thread_local std::map<const char*, std::string> cache;
    auto                                            found = cache.find(ptr);
    if (found == cache.end()) {
        found = cache.emplace(ptr, stylize(ptr)).first;
    }
    return found->second;
}
