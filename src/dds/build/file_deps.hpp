#pragma once

/**
 * The `file_deps` module implements the interdependencies of inputs files to their outputs, as well
 * as the command that was used to generate that output from the inputs.
 *
 * For a given output, there is exactly one command that was used to generate it, and some non-zero
 * number of input relations. A single input relation encapsulates the path to that input as well as
 * the file modification time at which that input was used. The modification times are specifically
 * stored on the input relation, and not associated with the input file itself, as more than one
 * output may make use of a single input, and each output will need to keep track of the
 * outdated-ness of its inputs separately.
 *
 * A toolchain has an associated `file_deps_mode`, which can be deduced from the compiler_id. The
 * three dependency modes are:
 *
 * 1. None - No dependency tracking takes place.
 * 2. GNU-Style - Dependencies are tracked using Clang and GCC's -M flags, which write a
 * Makefile-syntax file which contains the dependencies of the file that is being compiled. This
 * file is generated at the same time that the primary output is generated, and does not occur in a
 * pre-compile dependency pass.
 * 2. MSVC-Style - Dependencies are tracked using the cl.exe /showIncludes flag, which writes the
 * path of every file that is read by the preprocsesor to the compiler's output. This also happens
 * at the same time as main compilation, and does not require a pre-scan pass. Unfortunately, MSVC
 * localizes this string, so we cannot properly track dependencies without knowing what language it
 * will emit beforehand. At the moment, we implement dependency tracking for English, but providing
 * other languages is not difficult.
 */

#include <dds/db/database.hpp>
#include <dds/util/fs/path.hpp>

#include <neo/out.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace dds {

/**
 * The mode in which we can scan for compilation dependencies.
 */
enum class file_deps_mode {
    /// Disable dependency tracking
    none,
    /// Track dependencies using MSVC semantics
    msvc,
    /// Track dependencies using GNU-style generated-Makefile semantics
    gnu,
};

/**
 * The result of performing a dependency scan. A simple aggregate type.
 */
struct file_deps_info {
    /**
     * The primary output path.
     */
    fs::path output;
    /**
     * The paths to each input
     */
    std::vector<fs::path> inputs;
    /**
     * The command that was used to generate the output
     */
    completed_compilation command;
};

class database;

/**
 * Parse a compiler-generated Makefile that contains dependency information.
 * @see `parse_mkfile_deps_str`
 */
file_deps_info parse_mkfile_deps_file(path_ref where);

/**
 * Parse a Makefile-syntax string containing compile-generated dependency
 * information.
 * @param str A Makefile-syntax string that will be parsed.
 * @note The returned `file_deps_info` object will only have the `output` and
 * `inputs` fields filled in, as the other parameters cannot be deduced from
 * the Makefile. It is on the caller to fill these fields before passing them
 * to `update_deps_info`
 */
file_deps_info parse_mkfile_deps_str(std::string_view str);

/**
 * The result of parsing MSVC output for dependencies
 */
struct msvc_deps_info {
    /// The actual dependency information
    file_deps_info deps_info;
    /// The output from the MSVC compiler that has had the dependency information removed.
    std::string cleaned_output;
};

/**
 * Parse the output of the CL.exe compiler for file dependencies.
 * @param output The output from `cl.exe` that has had the /showIncludes flag set
 * @param leader The text prefix for each line that contains a dependency.
 * @note The returned `file_deps_info` object only has the `input_files` field set, and does not
 * include the primary input to the compiler. It is up to the caller to add the necessary fields and
 * values.
 * @note The `leader` parameter is localized depending on the language that `cl.exe` will use. In
 * English, this string is `Note: including file:`. If a line begins with this string, the remainder
 * of the line will be assumed to be a path to the file that the preprocessor read while compiling.
 * If the `leader` string does not match the language that `cl.exe` emits, then this parsing will
 * not see any of these notes, no dependencies will be seen, and the `cleaned_output` field in the
 * return value will still contain the /showIncludes notes.
 */
msvc_deps_info parse_msvc_output_for_deps(std::string_view output, std::string_view leader);

/**
 * Update the dependency information in the build database for later reference via
 * `get_prior_compilation`.
 * @param db The database to update
 * @param info The dependency information to store
 */
void update_deps_info(neo::output<database> db, const file_deps_info& info);

/**
 * The information that is pertinent to the rebuild of a file. This will contain a list of inputs
 * that have a newer mtime than we have recorded, and the previous command and previous command
 * output that we have stored.
 */
struct prior_compilation {
    std::vector<fs::path> newer_inputs;
    completed_compilation previous_command;
};

/**
 * Given the path to an output file, read all the dependency information from the database. If the
 * given output has never been recorded, then the resulting object will be null.
 */
std::optional<prior_compilation> get_prior_compilation(const database& db, path_ref output_path);

}  // namespace dds
