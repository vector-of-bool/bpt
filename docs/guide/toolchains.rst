.. highlight:: yaml
.. default-domain:: schematic
.. default-role:: schematic:prop

.. |--toolchain| replace:: :option:`--toolchain <bpt build --toolchain>`

Toolchains
##########

One of the core components of |bpt| is that of the *toolchain*. A toolchain
encompasses the environment used to :term:`compile` and :term:`link <linking>`
source code, including, but not limited to:

1. The executable binaries that constitute the language implementation:
   :term:`Compilers <compiler>`, :term:`linkers <linker>`, and archive managers.
2. The configuration of those tools, including most
   :term:`command-line arguments` given to those tools when they are invoked.
3. The set of preprocessor macros and language features that are active during
   compilation.

When a build is run, every file in the entire tree (including dependencies)
will be compiled, archived, and linked using the same toolchain.

This page provides an introduction on how one can make use of toolchains most
effectively in your project.

.. note::

    **IMPORTANT**: |bpt| will *not* automatically load the Visual C++
    environment. To use Visual C++, |bpt| must be executed from the appropriate
    environment in order for the Visual C++ toolchain executables and files to
    be available.


.. _toolchains.file:

Passing a Toolchain
*******************

In |bpt|, the default format of a toolchain is that of a single :term:`YAML`
file that describes the entire toolchain. When running a build for a project,
the |bpt| executable will look in a few locations for a default toolchain, and
generate an error if no default toolchain file is found (Refer to
:ref:`toolchains.default`). A different toolchain can be provided by passing the
toolchain file for the |--toolchain| (or ``-t``) option on the command line::

    $ bpt build -t my-toolchain.yaml

Alternatively, you can pass the name of a built-in toolchain. See below.


.. _toolchains.builtin:

Built-in Toolchains
*******************

For convenience, |bpt| includes several built-in toolchains that can be
accessed in the |--toolchain| command-line option using a colon ``:``
prefix::

    $ bpt build -t :gcc

|bpt| will treat the leading colon (``:``) as a name for a built-in toolchain.
There are several built-in toolchains that may be specified:

``:gcc``
    Uses the default ``gcc`` and ``g++`` executables, linkers, and options
    thereof.

``:gcc-N`` (for some integer ``N``)
    Equivalent to ``:gcc``, but uses the ``gcc-N`` and ``g++-N`` executables.

``:clang``
    Equivalent to ``:gcc``, but uses the ``clang`` and ``clang++`` executables.

``:clang-N`` (for some integer ``N``)
    Equivalent to ``:clang``, but uses the ``clang-N`` and ``clang++-N``
    executables.

``:msvc``
    Compiles and links using the Visual C++ toolchain.

The following pseudo-toolchains are also available:

``:debug:XYZ``
    Uses built-in toolchain ``:XYZ``, but generates debugging information.

``:ccache:XYZ``
    Uses built-in toolchain ``:XYZ``, but prefixes all compile commands with
    ``ccache``.

``:c++UV:XYZ`` (for two integers ``UV``)
    Sets the C++ version to ``C++UV`` and uses the ``:XYZ`` toolchain.


.. _toolchains.default:

Providing a Default Toolchain File
**********************************

If you do not wish to provide a new toolchain for every individual project, and
the built-in toolchains do not suit your needs, you can write a toolchain file
to one of a few predefined paths, and |bpt| will find and use it for the build.
The following directories are searched, in order:

#. ``$pwd/`` - If the working directory contains a toolchain file, it will be
   used as the default.
#. ``<bpt_config_dir>/`` - Searches for a toolchain file in |bpt|'s user-local
   configuration directory (see below).
#. ``<user_home>/`` - Searches for a toolchain file at the root of the current
   user's home directory. (``$HOME`` on Unix-like systems, and ``$PROFILE`` on
   Windows.)

In each directory, it will search for ``toolchain.yaml``, ``toolchain.json5``,
``toolchain.jsonc``, or ``toolchain.json``.

The ``<bpt_config_dir>`` directory is the |bpt| subdirectory of the user-local
configuration directory.

The user-local config directory is ``$XDG_CONFIG_DIR`` or ``~/.config`` on
Linux, ``~/Library/Preferences`` on macOS, and ``~/AppData/Roaming`` on Windows.


Toolchain Definitions
*********************

Besides using the built-in toolchains, it is likely that you'll soon want to
customize a toolchain further. Further customization must be done with a
file that contains the toolchain definition. The most basic toolchain file is
simply one line:

.. code-block::

    {
        compiler_id: "<compiler-id>"
    }

where ``<compiler-id>`` is one of the known |compiler_id| options. |bpt| will
infer common suitable defaults for the remaining options based on the value of
|compiler_id|.

For example, if you specify ``gnu``, then |bpt| will assume ``gcc`` to be the C
compiler, ``g++`` to be the C++ compiler, and ``ar`` to be the library archiving
tool.

If you know that your compiler executable has a different name, you can specify
them with additional options:

.. code-block::

    {
        compiler_id: 'gnu',
        c_compiler: 'gcc-9',
        cxx_compiler: 'g++-9',
    }

|bpt| will continue to infer other options based on the
:prop:`~ToolchainOptions.compiler_id`, but will use the provided executable
names when compiling files for the respective languages.

To specify compilation flags, the `~ToolchainOptions.flags` option can be used:

.. code-block::

    {
        // [...]
        flags: '-fsanitize=address -fno-inline',
    }

.. note::

    Use `~ToolchainOptions.warning_flags` to specify options regarding compiler
    warnings.

Flags for linking executables can be specified with
`~ToolchainOptions.link_flags`:

.. code-block::

    {
        // [...]
        link_flags: '-fsanitize=address -fPIE'
    }


.. note::

  Command/flag list settings are subject to shell-like string splitting. String
  splitting can be suppressed by using an array instead of a string. Refer:
  :ref:`tc-splitting-note`.

.. _toolchains.opt-ref:

Toolchain Option Reference
**************************

.. _tc-splitting-note:

Understanding Flags and Shell Parsing
=====================================

Many of the |bpt| toolchain parameters accept argument lists or shell-string
lists. If such an option is given a single string, then that string is split
using the syntax of a POSIX shell command parser. It accepts both single ``'``
and double ``"`` quote characters as argument delimiters.

If an option is given a list of strings instead, then each string in that
array is treated as a full command line argument and is passed as such.

For example, this sample with `~ToolchainOptions.flags`::

    {
        flags: "-fsanitize=address -fPIC"
    }

is equivalent to this one::

    {
        flags: ["-fsanitize=address", "-fPIC"]
    }

Despite splitting strings as-if they were shell commands, |bpt| does nothing
else shell-like. It does not expand environment variables, nor does it expand
globs and wildcards.


Toolchain Options Schema
========================

.. mapping:: ToolchainOptions

  .. property:: compiler_id
    :optional:

    :type: :ts:`"gnu" | "clang" | "msvc"`

    Specify the identity of the compiler. This option is used to infer many
    other facts about the toolchain. If specifying the full toolchain with the
    command templates, this option is not required.

    :option "gnu": For GCC
    :option "clang": For LLVM/Clang
    :option "msvc": For Microsoft Visual C++

    .. |compiler_id| replace:: :schematic:prop:`~ToolchainOptions.compiler_id`

    .. |gnu| replace:: :ts:`"gnu"`
    .. |clang| replace:: :ts:`"clang"`
    .. |msvc| replace:: :ts:`"msvc"`

    .. |default-inferred-from-compiler_id| replace::

        Inferred from the value of |compiler_id|


  .. property:: c_compiler
    :optional:
  .. property:: cxx_compiler
    :optional:

    :type: :ts:`string`

    Names/paths of the C and C++ compilers, respectively.

    :default: |default-inferred-from-compiler_id|:

      - When |compiler_id| is |gnu|:

        - :prop:`c_compiler`: ``gcc``
        - :prop:`cxx_compiler`: ``g++``

      - When |compiler_id| is |clang|:

        - :prop:`c_compiler`: ``clang``
        - :prop:`cxx_compiler`: ``clang++``

      - When |compiler_id| is |msvc|: ``cl.exe`` (for both C and C++)

  .. property:: c_version
    :optional:
  .. property:: cxx_version
    :optional:

    :type: :ts:`string`

    Specify the language versions for C and C++, respectively. By default, |bpt|
    will not set any language version. Using this option requires that the
    :prop:`~ToolchainOptions.compiler_id` be specified (Or the
    :prop:`~AdvancedToolchainOptions.lang_version_flag_template` advanced
    setting).

    Examples of :yaml:`c_version` values are:

    - ``c89``
    - ``c99``
    - ``c11``
    - ``c18``

    Examples of :yaml:`cxx_version` values are:

    - ``c++14``
    - ``c++17``
    - ``c++20``

    The given string will be substituted in the appropriate compile flag to
    specify the language version being passed.

    To enable GNU language extensions on GNU compilers, one can values like
    ``gnu++20``, which will result in ``-std=gnu++20`` being passed. Likewise,
    if the language version is "experimental" in your GCC release, you may set
    :yaml:`cxx_version` to the appropriate experimental version name, e.g.
    ``"c++2a"`` for ``-std=c++2a``.

    For MSVC, setting :yaml:`cxx_version` to ``c++latest`` will result in
    ``/std:c++latest``. **Beware** that this is an unstable setting value that
    could change the major language version in a future MSVC update.


  .. property:: warning_flags
    :optional:

    :type: :ts:`string | string[]`
    :default: :ts:`[]`

    Provide *additional* compiler flags that should be used to enable warnings.
    This option is stored separately from :prop:`flags`, as these options may be
    enabled/disabled separately depending on how |bpt| is invoked.

    .. note::

      If :prop:`~ToolchainOptions.compiler_id` is provided, a default set of
      warning flags will be provided when warnings are enabled.

      Adding flags to this toolchain option will *append* flags to the basis
      warning flag list rather than overwrite them.

    .. seealso::

      Refer to :prop:`~AdvancedToolchainOptions.base_warning_flags` for more
      information.


  .. property:: flags
    :optional:
  .. property:: c_flags
    :optional:
  .. property:: cxx_flags
    :optional:

    :type: :ts:`string | string[]`
    :default: :ts:`[]`

    Specify *additional* compiler options, possibly per-language. :yaml:`flags`
    will apply to all languages.

  .. property:: link_flags
    :optional:

    :type: :ts:`string | string[]`
    :default: :ts:`[]`

    Specify *additional* link options to use when linking executables.

    .. note::

      |bpt| does not invoke the linker directly, but instead invokes the
      compiler with the appropriate flags to perform linking. If you need to
      pass flags directly to the linker, you will need to use the compiler's
      options to direct flags through to the linker. On GNU-style, this is
      ``-Wl,<linker-option>``. With MSVC, a separate flag ``/LINK`` must be
      specified, and all following options are passed to the underlying
      ``link.exe``.


  .. property:: optimize
    :optional:

    :type: :ts:`boolean`
    :default: |false|

    Enable/disable compiler optimizations.


  .. property:: debug
    :optional:

    :type: :ts:`boolean | "embedded" | "split"`
    :default: |false|

    :option "embedded":

      Generates debug information embedded in the compiled binaries.

    :option "split":

      Generates debug information in a separate file from the compiled binaries.

      .. note::

        ``"split"`` with GCC/Clang requires that the compiler support the
        ``-gsplit-dwarf`` option.

    :option true: Same as :ts:`"embedded"`

    :option false: Do not generate any debug information.


  .. property:: runtime
    :optional:

    :type: :ts:`{static?: boolean, debug?: boolean}`

    Select the language runtime/standard library options. Must be an object, and
    supports two sub-properties:

    .. property:: static
      :optional:

      :type: boolean

      A boolean. If |true|, the runtime and standard libraries will be
      static-linked into the generated binaries. If |false|, they will be
      dynamically linked. Default is |true| with MSVC, and |false| with GCC and
      Clang.

    .. property:: debug
      :optional:

      :type: boolean
      :default:

        - If |compiler_id| is |msvc|, the default value depends on the top-level
          :prop:`ToolchainOptions.debug` option: If :yaml:`debug` is not
          |false|, then :yaml:`runtime.debug` defaults to |true|.
        - Otherwise, the default value is |false|.

      A boolean. If |true|, the debug versions of the runtime and standard
      library will be compiled and linked into the generated binaries. If
      |false|, the default libraries will be used.

    .. note::

      On GNU-like compilers, setting :prop:`~ToolchainOptions.runtime.static` to
      |true| does not generate a static executable: it only statically links the
      runtime and standard library. To generate a static executable, the
      ``-static`` option should be added to ``link_flags``.

    .. note::

      On GNU and Clang, setting :yaml:`runtime.debug` to |true| will compile all
      files with the ``_GLIBCXX_DEBUG`` and ``_LIBCPP_DEBUG=1`` preprocessor
      definitions set. **Translation units compiled with these macros are
      definitively ABI-incompatible with TUs that have been compiled without
      these options!!**

      If you link to a static or dynamic library that has not been compiled with
      the same runtime settings, generated programs will likely crash.


  .. property:: compiler_launcher
    :optional:

    :type: :ts:`string | string[]`

    Provide a command prefix that should be used on all compiler executions.
    e.g. ``ccache``.

  .. property:: advanced
    :optional:

    :type: AdvancedToolchainOptions

    A nested object that contains advanced toolchain options. These settings
    should be handled with care.


Advanced Options Reference
**************************

.. mapping:: AdvancedToolchainOptions

  The options below are probably not good to tweak unless you *really* know what
  you are doing. Their values will be inferred from
  :prop:`~ToolchainOptions.compiler_id`.

  .. _command template:

  .. rubric:: Command Templates

  Many of the below options take the form of command-line templates. These are
  templates from which |bpt| will create a command-line for a subprocess,
  possibly by combining them together.

  Each command template allows some set of placeholders. Each instance of the
  placeholder string will be replaced in the final command line. Refer to each
  respective option for more information.

  .. property:: deps_mode
    :optional:

    :type: :ts:`"gnu" | "msvc" | "none"`

    Specify the way in which |bpt| should track compilation dependencies. One of
    ``gnu``, ``msvc``, or ``none``.

    :default: |default-inferred-from-compiler_id|.

    .. note::

      If ``none``, then dependency tracking will be disabled entirely. This will
      prevent |bpt| from tracking interdependencies of source files, and
      inhibits incremental compilation.

  .. property:: c_compile_file
    :optional:
  .. property:: cxx_compile_file
    :optional:

    :type: :ts:`string | string[]`

    Override the `command template`_ that is used to compile source files.

    :placeholder [in]: The path to the source file that will be compiled.
    :placeholder [out]: The path to the object file that will be generated.

    :placeholder [flags]:

      The placeholder of the compilation flags. This placeholder must not be
      attached to any other arguments. The compilation flag argument list will
      be inserted in place of ``[flags]``.

    :default: |default-inferred-from-compiler_id|:

      - If |compiler_id| is |msvc|, then:

        - :prop:`c_compile_file` is :ts:`<c_compiler> <base_flags> [flags] /c [in] /Fo[out]`
        - :prop:`cxx_compile_file` is :ts:`<cxx_compiler> <base_flags> [flags] /c [in] /Fo[out]`

      - If |compiler_id| is |gnu| or |clang|, then:

        - :prop:`c_compile_file` is :ts:`<c_compiler> <base_flags> [flags] -c [in] -o[out]`
        - :prop:`cxx_compile_file` is :ts:`<cxx_compiler> <base_flags> [flags] -c [in] -o[out]`

      - If |compiler_id| is unset, then this property must be specified.


  .. property:: create_archive
    :optional:

    :type: :ts:`string | string[]`

    Override the `command template`_ that is used to generate static library
    archive files.

    :placeholder [in]:

      The list of inputs. It must not be attached to any other arguments. The
      list of input paths will be inserted in place of ``[in]``.

    :placeholder [out]:

      The placeholder for the output path for the static library archive.

    :default: |default-inferred-from-compiler_id|:

      - If |compiler_id| is |msvc|, then ``lib /nologo /OUT:[out] [in]``
      - If |compiler_id| is |gnu| or |clang|, then ``ar rcs [out] [in]``
      - If |compiler_id| is unset, then this property must be specified.


  .. property:: link_executable
    :optional:

    :type: :ts:`string | string[]`

    Override the `command template`_ that is used to link executables.

    :placeholder [in]:

      The list of input filepaths. It must not be attached to any other
      arguments. The list of input paths will be inserted in place of ``[in]``.

    :placeholder [out]:

      The placeholder for the output path for the executable file.

    :placeholder [flags]:

      Placeholder for options specified using `~ToolchainOptions.link_flags`.

    :default: |default-inferred-from-compiler_id|:

      - If |compiler_id| is |msvc|, then
        ``<cxx_compiler> /nologo /EHsc [in] /Fe[out] [flags]``
      - If |compiler_id| is |gnu| or |clang|, then
        ``<cxx_compiler> -fPIC [in] -pthread -o[out] [flags]``
      - If |compiler_id| is unset, then this property must be specified.


  .. property:: include_template
    :optional:
  .. property:: external_include_template
    :optional:

    :type: :ts:`string | string[]`

    Override the `command template`_ for the flags to specify a header search
    path. ``external_include_template`` will be used to specify the include
    search path for a directory that is "external" (i.e. does not live within
    the main project).

    For each directory added to the ``#include`` search path, this argument
    template is instantiated in the ``[flags]`` for the compilation.

    :placeholder [path]:

      The path to the directory to be added to the search path.

    :default: |default-inferred-from-compiler_id|:

      - If |compiler_id| is |msvc|, then ``/I [path]``.
      - If |compiler_id| is |gnu| or |clang|:
        - :prop:`include_template` is ``-I [path]``.
        - :prop:`extern_include_template` is ``-isystem [path]``.
      - If |compiler_id| is unset, then this property must be specified.


  .. property:: define_template
    :optional:

    :type: :ts:`string | string[]`

    Override the `command template`_ for the flags to set a preprocessor
    definition.

    :placeholder [def]:

      The preprocessor macro definition to define.

    :default: |default-inferred-from-compiler_id|:

      - If |compiler_id| is |msvc|, then ``/D [path]``.
      - If |compiler_id| is |gnu| or |clang|, then ``-D [path]``.
      - If |compiler_id| is unset, then this property must be specified.


  .. property:: lang_version_flag_template
    :optional:

    :type: :ts:`string|string[]`

    Set the flag template string for the language-version specifier for the
    compiler command line.

    :placeholder [version]:

      The version string passed for :prop:`c_version` or :prop:`cxx_version`.

    This template expects a single placeholder: ``[version]``, which is

    On MSVC, this defaults to ``/std:[version]``. On GNU-like compilers, it
    defaults to ``-std=[version]``.


  .. property:: tty_flags
    :optional:

    :type: :ts:`string|string[]`

    Supply additional flags when compiling/linking that will only be applied if
    standard output is an ANSI-capable terminal.

    On GNU and Clang this will be ``-fdiagnostics-color`` by default.


  .. property:: obj_prefix
    :optional:
  .. property:: obj_suffix
    :optional:
  .. property:: archive_prefix
    :optional:
  .. property:: archive_suffix
    :optional:
  .. property:: exe_prefix
    :optional:
  .. property:: exe_suffix
    :optional:

    :type: :ts:`string`

    Set the filename prefixes and suffixes for object files, library archive
    files, and executable files, respectively.

    :default:

      |default-inferred-from-compiler_id| and the host system on which |bpt| is
      executing.


  .. property:: base_warning_flags
    :optional:

    :type: :ts:`string | string[]`

    When you compile your project and request warning flags, |bpt| will
    concatenate the warning flags from this option with the flags provided by
    `warning_flags`. This option is "advanced," because it provides a set of
    defaults based on the :prop:`~ToolchainOptions.compiler_id`.

    On GNU-like compilers, the base warning flags are
    ``-Wall -Wextra -Wpedantic -Wconversion``. On MSVC the default flag is
    ``/W4``.

    For example, if you set `warning_flags` to ``"-Werror"`` on a GNU-like
    compiler, the resulting command line will contain
    ``-Wall -Wextra -Wpedantic -Wconversion -Werror``.


  .. property:: base_flags
    :optional:
  .. property:: base_c_flags
    :optional:
  .. property:: base_cxx_flags
    :optional:

    :type: :ts:`string | string[]`

    When you compile your project, |bpt| uses a set of default flags appropriate
    to the target language and compiler. These flags are always included in the
    compile command and are inserted in addition to those flags provided by
    `flags`.

    On GNU-like compilers, the base flags are ``-fPIC -pthread``. On MSVC the
    default flags are ``/EHsc /nologo /permissive-`` for C++ and
    ``/nologo /permissive-`` for C.

    These defaults may be changed by providing values for three different
    options. The ``base_flags`` value is always output, regardless of language.
    Flags exclusive to C are specified in ``base_c_flags``, and those
    exclusively for C++ should be in ``base_cxx_flags``. Note that the
    language-specific values are independent from ``base_flags``; that is,
    providing ``base_c_flags`` or ``base_cxx_flags`` does not override or
    prevent the inclusion of the ``base_flags`` value, and vice-versa. Empty
    values are acceptable, should you need to simply prohibit one or more of the
    defaults from being used.

    For example, if you set `ToolchainOptions.flags` to ``-ansi`` on a GNU-like
    compiler, the resulting command line will contain ``-fPIC -pthread -ansi``.
    If, additionally, you set ``base_flags`` to ``-fno-builtin`` and
    ``base_cxx_flags`` to ``-fno-exceptions``, the generated command will
    include ``-fno-builtin -fno-exceptions -ansi`` for C++ and
    ``-fno-builtin -ansi`` for C.
