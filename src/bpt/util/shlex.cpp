#include "./shlex.hpp"

#include <optional>
#include <string>
#include <utility>

using std::string;
using std::vector;

using namespace dds;

vector<string> dds::split_shell_string(std::string_view shell) {
    char cur_quote  = 0;
    bool is_escaped = false;

    vector<string> acc;

    const auto begin = shell.begin();
    auto       iter  = begin;
    const auto end   = shell.end();

    std::optional<string> token;
    while (iter != end) {
        const char c = *iter++;
        if (is_escaped) {
            if (c == '\n') {
                // Ignore the newline
            } else if (cur_quote || c != cur_quote || c == '\\') {
                // Escaped `\` character
                token = token.value_or("") + c;
            } else {
                // Regular escape sequence
                token = token.value_or("") + '\\' + c;
            }
            is_escaped = false;
        } else if (c == '\\') {
            is_escaped = true;
        } else if (cur_quote) {
            if (c == cur_quote) {
                // End of quoted token;
                cur_quote = 0;
            } else {
                token = token.value_or("") + c;
            }
        } else if (c == '"' || c == '\'') {
            // Beginning of a quoted token
            cur_quote = c;
            token     = "";
        } else if (c == '\t' || c == ' ' || c == '\n' || c == '\r' || c == '\f') {
            // We've reached unquoted whitespace
            if (token.has_value()) {
                acc.push_back(move(*token));
            }
            token.reset();
        } else {
            // Just a regular character
            token = token.value_or("") + c;
        }
    }

    if (token.has_value()) {
        acc.push_back(move(*token));
    }

    return acc;
}