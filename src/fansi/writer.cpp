#include "./writer.hpp"

#include <neo/buffer_algorithm/concat.hpp>

#include <array>
#include <charconv>

using namespace fansi;
using namespace neo::literals;

namespace {

int code_for_color(std_color col, bool bright) {
    return 30 + int(col) + ((bright && col != std_color::normal) ? 60 : 0);
}

}  // namespace

void text_writer::put_style(const text_style& new_style) noexcept {
    auto&       prev_style        = _style;
    bool        unbold            = false;
    std::string reset_then_enable = "0";
    std::string set_toggles;

    using neo::dynbuf_concat;

    auto append_int = [&](std::string& out, int i) {
        std::array<char, 4> valbuf;
        auto                res = std::to_chars(valbuf.data(), valbuf.data() + sizeof(valbuf), i);
        if (!out.empty()) {
            out.push_back(';');
        }
        neo::dynbuf_concat(out, neo::as_buffer(valbuf, res.ptr - valbuf.data()));
    };

    auto append_toggle = [&](bool my_state, bool prev_state, int on_val) {
        int off_val = on_val + 20;
        if (!my_state) {
            if (prev_state != my_state) {
                append_int(set_toggles, off_val);
                if (off_val == 21) {
                    // ! Hack: Terminals disagree on the meaning of 21. ECMA says
                    // "double-underline", but intuition tells us it would be bold-off, since it is
                    // SGR Bold [1] plus twenty, as with all other toggles.
                    unbold = true;
                }
            }
        } else {
            append_int(reset_then_enable, on_val);
            if (prev_state != my_state) {
                append_int(set_toggles, on_val);
            }
        }
    };

    append_toggle(new_style.bold, prev_style.bold, 1);
    append_toggle(new_style.faint, prev_style.faint, 2);
    append_toggle(new_style.italic, prev_style.italic, 3);
    append_toggle(new_style.underline, prev_style.underline, 4);
    append_toggle(new_style.reverse, prev_style.reverse, 7);
    append_toggle(new_style.strike, prev_style.strike, 9);

    int fg_int      = code_for_color(new_style.fg_color, new_style.bright);
    int bg_int      = code_for_color(new_style.bg_color, new_style.bright) + 10;
    int prev_fg_int = code_for_color(prev_style.fg_color, prev_style.bright);
    int prev_bg_int = code_for_color(prev_style.bg_color, prev_style.bright) + 10;

    if (new_style.fg_color == std_color::normal) {
        // No need to change the foreground color for the reset, but maybe for the toggle
        if (fg_int != prev_fg_int) {
            append_int(set_toggles, fg_int);
        }
    } else {
        append_int(reset_then_enable, fg_int);
        if (fg_int != prev_fg_int) {
            append_int(set_toggles, fg_int);
        }
    }

    if (new_style.bg_color == std_color::normal) {
        // No need to change the background color for the reset, but maybe for the toggle
        if (bg_int != prev_bg_int) {
            append_int(set_toggles, bg_int);
        }
    } else {
        append_int(reset_then_enable, bg_int);
        if (bg_int != prev_bg_int) {
            append_int(set_toggles, bg_int);
        }
    }

    if (set_toggles.empty()) {
        // No changes necessary
    } else if (unbold || set_toggles.size() > reset_then_enable.size()) {
        dynbuf_concat(_buf, "\x1b[", reset_then_enable, "m");
    } else {
        dynbuf_concat(_buf, "\x1b[", set_toggles, "m");
    }

    _style = new_style;
}
