#include <dds/build/plan/template.hpp>

#include <dds/error/errors.hpp>
#include <dds/sdist/library/root.hpp>
#include <dds/util/fs.hpp>
#include <dds/util/string.hpp>

#include <ctre.hpp>
#include <semester/json.hpp>

#include <string>
#include <string_view>

using namespace dds;

using json_data = semester::json_data;

namespace {

static constexpr ctll::fixed_string IDENT_RE{"([_a-zA-Z]\\w*)(.*)"};

std::string_view skip(std::string_view in) {
    auto nspace_pos = in.find_first_not_of(" \t\n\r\f");
    in              = in.substr(nspace_pos);
    if (starts_with(in, "/*")) {
        // It's a block comment. Find the block-end marker.
        auto block_end = in.find("*/");
        if (block_end == in.npos) {
            throw_user_error<errc::template_error>("Unterminated block comment");
        }
        in = in.substr(block_end + 2);
        // Recursively skip some more
        return skip(in);
    }
    if (starts_with(in, "//")) {
    more:
        // It's a line comment. Find the next not-continued newline
        auto cn_nl = in.find("\\\n");
        auto nl    = in.find("\n");
        if (cn_nl < nl) {
            // The next newline is a continuation of the comment. Keep looking
            in = in.substr(nl + 1);
            goto more;
        }
        if (nl == in.npos) {
            // We've reached the end. Okay.
            return in.substr(nl);
        }
    }
    // Not a comment, and not whitespace. Okay.
    return in;
}

std::string stringify(const json_data& dat) {
    if (dat.is_bool()) {
        return dat.as_bool() ? "true" : "false";
    } else if (dat.is_double()) {
        return std::to_string(dat.as_double());
    } else if (dat.is_null()) {
        return "nullptr";
    } else if (dat.is_string()) {
        /// XXX: This probably isn't quite enough sanitization for edge cases.
        auto str = dat.as_string();
        str      = replace(str, "\n", "\\n");
        str      = replace(str, "\"", "\\\"");
        return "\"" + str + "\"";
    } else {
        throw_user_error<errc::template_error>("Cannot render un-stringable data type");
    }
}

std::pair<std::string, std::string_view> eval_expr_tail(std::string_view in, const json_data& dat) {
    in = skip(in);
    if (starts_with(in, ".")) {
        // Accessing a subproperty of the data
        in.remove_prefix(1);
        in = skip(in);
        // We _must_ see an identifier
        auto [is_ident, ident, tail] = ctre::match<IDENT_RE>(in);
        if (!is_ident) {
            throw_user_error<errc::template_error>("Expected identifier following dot `.`");
        }
        if (!dat.is_mapping()) {
            throw_user_error<errc::template_error>("Cannot use dot `.` on non-mapping object");
        }
        auto& map   = dat.as_mapping();
        auto  found = map.find(ident.to_view());
        if (found == map.end()) {
            throw_user_error<errc::template_error>("No subproperty '{}'", ident.to_view());
        }
        return eval_expr_tail(tail, found->second);
    }
    return {stringify(dat), in};
}

std::pair<std::string, std::string_view> eval_primary_expr(std::string_view in,
                                                           const json_data& dat) {
    in = skip(in);

    if (in.empty()) {
        throw_user_error<errc::template_error>("Expected primary expression");
    }

    if (in.front() == '(') {
        in               = in.substr(1);
        auto [ret, tail] = eval_primary_expr(in, dat);
        if (!starts_with(tail, ")")) {
            throw_user_error<errc::template_error>(
                "Expected closing parenthesis `)` following expression");
        }
        return {ret, tail.substr(1)};
    }

    auto [is_ident, ident, tail_1] = ctre::match<IDENT_RE>(in);

    if (is_ident) {
        auto& map   = dat.as_mapping();
        auto  found = map.find(ident.to_view());
        if (found == map.end()) {
            throw_user_error<errc::template_error>("Unknown top-level identifier '{}'",
                                                   ident.to_view());
        }

        return eval_expr_tail(tail_1, found->second);
    }

    return {"nope", in};
}

std::string render_template(std::string_view tmpl, const library_root& lib) {
    std::string      acc;
    std::string_view MARKER_STRING = "__dds";

    // Fill out a data structure that will be exposed to the template
    json_data dat = json_data::mapping_type({
        {
            "lib",
            json_data::mapping_type{
                {"name", lib.manifest().name},
                {"root", lib.path().string()},
            },
        },
    });

    while (!tmpl.empty()) {
        // Find the next marker in the template string
        auto next_marker = tmpl.find(MARKER_STRING);
        if (next_marker == tmpl.npos) {
            // We've reached the end of the template. Stop
            acc.append(tmpl);
            break;
        }
        // Append the string up to the next marker
        acc.append(tmpl.substr(0, next_marker));
        // Consume up to the next marker
        tmpl                = tmpl.substr(next_marker + MARKER_STRING.size());
        auto next_not_space = tmpl.find_first_not_of(" \t");
        if (next_not_space == tmpl.npos || tmpl[next_not_space] != '(') {
            throw_user_error<errc::template_error>(
                "Expected `(` following `__dds` identifier in template file");
        }

        auto [inner, tail] = eval_primary_expr(tmpl, dat);
        acc.append(inner);
        tmpl = tail;
    }

    return acc;
}

}  // namespace

void render_template_plan::render(build_env_ref env, const library_root& lib) const {
    auto content = slurp_file(_source.path);

    // Calculate the destination of the template rendering
    auto dest = env.output_root / _subdir / _source.relative_path();
    dest.replace_filename(dest.stem().stem().filename().string() + dest.extension().string());
    fs::create_directories(dest.parent_path());

    auto result = render_template(content, lib);
    if (fs::is_regular_file(dest)) {
        auto existing_content = slurp_file(dest);
        if (result == existing_content) {
            /// The content of the file has not changed. Do not write a file.
            return;
        }
    }

    auto ofile = open(dest, std::ios::binary | std::ios::out);
    ofile << result;
    ofile.close();  // Throw any exceptions while closing the file
}
