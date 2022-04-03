.. highlight:: js

Toolchains
##########

One of the core components of ``bpt`` is that of the *toolchain*. A toolchain
encompasses the environment used to build and link source code, including, but
not limited to:

#. The executable binaries that constitute the language implementation:
   Compilers, linkers, and archive managers.
#. The configuration of those tools, including most options given to those
   tools when they are invoked.
#. The set of preprocessor macros and language features that are active during
   compilation.

When a build is run, every file in the entire tree (including dependencies)
will be compiled, archived, and linked using the same toolchain.

This page provides an introduction on how one can make use of toolchains most
effectively in your project.

.. note::
    **IMPORTANT**: ``bpt`` will *not* automatically load the Visual C++
    environment. To use Visual C++, ``bpt`` must be executed from the
    appropriate environment in order for the Visual C++ toolchain executables
    and files to be available.


Passing a Toolchain
*******************

In ``bpt``, the default format of a toolchain is that of a single JSON5 file
that describes the entire toolchain. When running a build for a project, the
``bpt`` executable will look in a few locations for a default toolchain, and
generate an error if no default toolchain file is found (Refer to
:ref:`toolchains.default`). A different toolchain can be provided by passing
the toolchain file for the ``--toolchain`` (or ``-t``) option on the command
line::

    $ bpt build -t my-toolchain.json5

Alternatively, you can pass the name of a built-in toolchain. See below.


.. _toolchains.builtin:

Built-in Toolchains
*******************

For convenience, ``bpt`` includes several built-in toolchains that can be
accessed in the ``--toolchain`` command-line option using a colon ``:``
prefix::

    $ bpt build -t :gcc

``bpt`` will treat the leading colon (``:``) as a name for a built-in
toolchain (this means that a toolchain's filepath may not begin with a colon).

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

If you do not wish to provide a new toolchain for every individual project,
and the built-in toolchains do not suit your needs, you can write a toolchain
file to one of a few predefined paths, and ``bpt`` will find and use it for the
build. The following directories are searched, in order:

#. ``$pwd/`` - If the working directory contains a toolchain file, it will be
   used as the default.
#. ``$bpt_config_dir/`` - Searches for a toolchain file in ``bpt``'s user-local
   configuration directory (see below).
#. ``$user_home/`` - Searches for a toolchain file at the root of the current
   user's home directory. (``$HOME`` on Unix-like systems, and ``$PROFILE`` on
   Windows.)

In each directory, it will search for ``toolchain.json5``, ``toolchain.jsonc``,
or ``toolchain.json``.

The ``$bpt_config_dir`` directory is the ``bpt`` subdirectory of the
user-local configuration directory.

The user-local config directory is ``$XDG_CONFIG_DIR`` or ``~/.config`` on
Linux, ``~/Library/Preferences`` on macOS, and ``~/AppData/Roaming`` on
Windows.


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

where ``<compiler-id>`` is one of the known ``compiler_id`` options (See the
toolchain option reference). ``bpt`` will infer common suitable defaults for
the remaining options based on the value of ``compiler_id``.

For example, if you specify ``gnu``, then ``bpt`` will assume ``gcc`` to be the
C compiler, ``g++`` to be the C++ compiler, and ``ar`` to be the library
archiving tool.

If you know that your compiler executable has a different name, you can
specify them with additional options:

.. code-block::

    {
        compiler_id: 'gnu',
        c_compiler: 'gcc-9',
        cxx_compiler: 'g++-9',
    }

``bpt`` will continue to infer other options based on the ``compiler_id``, but
will use the provided executable names when compiling files for the respective
languages.

To specify compilation flags, the ``flags`` option can be used:

.. code-block::

    {
        // [...]
        flags: '-fsanitize=address -fno-inline',
    }

.. note::
    Use ``warning_flags`` to specify options regarding compiler warnings.

Flags for linking executables can be specified with ``link_flags``:

.. code-block::

    {
        // [...]
        link_flags: '-fsanitize=address -fPIE'
    }


.. _toolchains.opt-ref:

Toolchain Option Reference
**************************


Understanding Flags and Shell Parsing
-------------------------------------

Many of the ``bpt`` toolchain parameters accept argument lists or shell-string
lists. If such an option is given a single string, then that string is split
using the syntax of a POSIX shell command parser. It accepts both single ``'``
and double ``"`` quote characters as argument delimiters.

If an option is given a list of strings instead, then each string in that
array is treated as a full command line argument and is passed as such.

For example, this sample with ``flags``::

    {
        flags: "-fsanitize=address -fPIC"
    }

is equivalent to this one::

    {
        flags: ["-fsanitize=address", "-fPIC"]
    }

Despite splitting strings as-if they were shell commands, ``bpt`` does nothing
else shell-like. It does not expand environment variables, nor does it expand
globs and wildcards.


``compiler_id``
---------------

Specify the identity of the compiler. This option is used to infer many other
facts about the toolchain. If specifying the full toolchain with the command
templates, this option is not required.

Valid values are:

``gnu``
    For GCC

``clang``
    For LLVM/Clang

``msvc``
    For Microsoft Visual C++


``c_compiler`` and ``cxx_compiler``
-----------------------------------

Names/paths of the C and C++ compilers, respectively. Defaults will be inferred
from ``compiler_id``.


``c_version`` and ``cxx_version``
---------------------------------

Specify the language versions for C and C++, respectively. By default, ``bpt``
will not set any language version. Using this option requires that the
``compiler_id`` be specified (Or the ``lang_version_flag_template`` advanced
setting).

Examples of ``c_version`` values are:

- ``c89``
- ``c99``
- ``c11``
- ``c18``

Examples of ``cxx_version`` values are:

- ``c++14``
- ``c++17``
- ``c++20``

The given string will be substituted in the appropriate compile flag to specify
the language version being passed.

To enable GNU language extensions on GNU compilers, one can values like
``gnu++20``, which will result in ``-std=gnu++20`` being passed. Likewise, if
the language version is "experimental" in your GCC release, you may set
``cxx_version`` to the appropriate experimental version name, e.g. ``"c++2a"``
for ``-std=c++2a``.

For MSVC, setting ``cxx_version`` to ``c++latest`` will result in
``/std:c++latest``. **Beware** that this is an unstable setting value that could
change the major language version in a future MSVC update.


``warning_flags``
-----------------

Provide *additional* compiler flags that should be used to enable warnings. This
option is stored separately from ``flags``, as these options may be
enabled/disabled separately depending on how ``bpt`` is invoked.

.. note::

    If ``compiler_id`` is provided, a default set of warning flags will be
    provided when warnings are enabled.

    Adding flags to this toolchain option will *append* flags to the basis
    warning flag list rather than overwrite them.

.. seealso::

    Refer to :ref:`toolchains.opts.base_warning_flags` for more information.


``flags``, ``c_flags``, and ``cxx_flags``
-----------------------------------------

Specify *additional* compiler options, possibly per-language.


``link_flags``
--------------

Specify *additional* link options to use when linking executables.

.. note::

    ``bpt`` does not invoke the linker directly, but instead invokes the
    compiler with the appropriate flags to perform linking. If you need to pass
    flags directly to the linker, you will need to use the compiler's options to
    direct flags through to the linker. On GNU-style, this is
    ``-Wl,<linker-option>``. With MSVC, a separate flag ``/LINK`` must be
    specified, and all following options are passed to the underlying
    ``link.exe``.


``optimize``
------------

Boolean option (``true`` or ``false``) to enable/disable optimizations. Default
is ``false``.


``debug``
---------

Bool or string. Default is ``false``. If ``true`` or ``"embedded"``, generates
debug information embedded in the compiled binaries. If ``"split"``, generates
debug information in a separate file from the binaries.

.. note::
    ``"split"`` with GCC requires that the compiler support the
    ``-gsplit-dwarf`` option.


``runtime``
-----------

Select the language runtime/standard library options. Must be an object, and supports two keys:

``static``
    A boolean. If ``true``, the runtime and standard libraries will be
    static-linked into the generated binaries. If ``false``, they will be
    dynamically linked. Default is ``true`` with MSVC, and ``false`` with GCC
    and Clang.

``debug``
    A boolean. If ``true``, the debug versions of the runtime and standard
    library will be compiled and linked into the generated binaries. If
    ``false``, the default libraries will be used.

    **On MSVC** the default value depends on the top-level ``/debug`` option: If
    ``/debug`` is not ``false``, then ``/runtime/debug`` defaults to ``true``.

    **On GCC and Clang** the default value is ``false``.

.. note::

    On GNU-like compilers, ``static`` does not generate a static executable, it
    only statically links the runtime and standard library. To generate a static
    executable, the ``-static`` option should be added to ``link_flags``.

.. note::

    On GNU and Clang, setting ``/runtime/debug`` to ``true`` will compile all
    files with the ``_GLIBCXX_DEBUG`` and ``_LIBCPP_DEBUG=1`` preprocessor
    definitions set. **Translation units compiled with these macros are
    definitively ABI-incompatible with TUs that have been compiled without these
    options!!**

    If you link to a static or dynamic library that has not been compiled with
    the same runtime settings, generated programs will likely crash.


``compiler_launcher``
---------------------

Provide a command prefix that should be used on all compiler executions.
e.g. ``ccache``.


``advanced``
------------

A nested object that contains advanced toolchain options. Refer to section on
advanced toolchain options.


Advanced Options Reference
**************************

The options below are probably not good to tweak unless you *really* know what
you are doing. Their values will be inferred from ``compiler_id``.


Command Templates
-----------------

Many of the below options take the form of command-line templates. These are
templates from which ``bpt`` will create a command-line for a subprocess,
possibly by combining them together.

Each command template allows some set of placeholders. Each instance of the
placeholder string will be replaced in the final command line. Refer to each
respective option for more information.


``deps_mode``
-------------

Specify the way in which ``bpt`` should track compilation dependencies. One
of ``gnu``, ``msvc``, or ``none``.

.. note::
    If ``none``, then dependency tracking will be disabled entirely. This will
    prevent ``bpt`` from tracking interdependencies of source files, and
    inhibits incremental compilation.


``c_compile_file`` and ``cxx_compile_file``
-------------------------------------------

Override the *command template* that is used to compile source files.

This template expects three placeholders:

- ``[in]`` is the path to the file that will be compiled.
- ``[out]`` is the path to the object file that will be generated.
- ``[flags]`` is the placeholder of the compilation flags. This placeholder
  must not be attached to any other arguments. The compilation flag argument
  list will be inserted in place of ``[flags]``.

Defaults::

    {
        // On GNU-like compilers (GCC, Clang):
        c_compile_file:   "<compiler> <base_flags> [flags] -c [in] -o[out]",
        cxx_compile_file: "<compiler> <base_flags> [flags] -c [in] -o[out]",

        // On MSVC:
        c_compile_file:   "cl.exe <base_flags> [flags] /c [in] /Fo[out]",
        cxx_compile_file: "cl.exe <base_flags> [flags] /c [in] /Fo[out]",
    }


``create_archive``
------------------

Override the *command template* that is used to generate static library archive
files.

This template expects two placeholders:

- ``[in]`` is the a placeholder for the list of inputs. It must not be attached
  to any other arguments. The list of input paths will be inserted in place of
  ``[in]``.
- ``[out]`` is the placeholder for the output path for the static library
  archive.

Defaults::

    {
        // On GNU-like:
        create_archive: "ar rcs [out] [in]",
        // On MSVC:
        create_archive: "lib /nologo /OUT:[out] [in]",
    }


``link_executable``
-------------------

Override the *command template* that is used to link executables.

This template expects the same placeholders as ``create_archive``, but
``[out]`` is a placeholder for the executable file rather than a static
library.

Defaults::

    {
        // For GNU-like:
        link_executable: "<compiler> -fPIC [in] -pthread -o[out] [flags]",
        // For MSVC:
        link_executable: "cl.exe /nologo /EHsc [in] /Fe[out]",
    }


``include_template`` and ``external_include_template``
------------------------------------------------------

Override the *command template* for the flags to specify a header search path.
``external_include_template`` will be used to specify the include search path
for a directory that is "external" (i.e. does not live within the main project).

For each directory added to the ``#include`` search path, this argument
template is instantiated in the ``[flags]`` for the compilation.

This template expects only a single placeholder: ``[path]``, which will be
replaced with the path to the directory to be added to the search path.

On MSVC, this defaults to ``/I [path]``. On GNU-like, ``-isystem [path]`` is
used for ``external_include_template`` and ``-I [path]`` for
``include_template``.


``define_template``
-------------------

Override the *command template* for the flags to set a preprocessor definition.

This template expects only a single placeholder: ``[def]``, which is the
preprocessor macro definition argument.

On MSVC, this defaults to ``/D [def]``. On GNU-like compilers, this is
``-D [def]``.


``lang_version_flag_template``
------------------------------

Set the flag template string for the language-version specifier for the
compiler command line.

This template expects a single placeholder: ``[version]``, which is the version
string passed for ``c_version`` or ``cxx_version``.

On MSVC, this defaults to ``/std:[version]``. On GNU-like compilers, it
defaults to ``-std=[version]``.


``tty_flags``
-------------

Supply additional flags when compiling/linking that will only be applied if
standard output is an ANSI-capable terminal.

On GNU and Clang this will be ``-fdiagnostics-color`` by default.


``obj_prefix``, ``obj_suffix``, ``archive_prefix``, ``archive_suffix``, ``exe_prefix``, and ``exe_suffix``
----------------------------------------------------------------------------------------------------------

Set the filename prefixes and suffixes for object files, library archive files,
and executable files, respectively.


.. _toolchains.opts.base_warning_flags:

``base_warning_flags``
----------------------

When you compile your project and request warning flags, ``bpt`` will
concatenate the warning flags from this option with the flags provided by
``warning_flags``. This option is "advanced," because it provides a set of
defaults based on the ``compiler_id``.

On GNU-like compilers, the base warning flags are ``-Wall -Wextra -Wpedantic
-Wconversion``. On MSVC the default flag is ``/W4``.

For example, if you set ``warning_flags`` to ``"-Werror"`` on a GNU-like
compiler, the resulting command line will contain ``-Wall -Wextra -Wpedantic
-Wconversion -Werror``.


.. _toolchains.opts.base_flags:

``base_flags``, ``base_c_flags``, and ``base_cxx_flags``
--------------------------------------------------------

When you compile your project, ``bpt`` uses a set of default flags appropriate
to the target language and compiler. These flags are always included in the
compile command and are inserted in addition to those flags provided by
``flags``, ``c_flags``, and ``cxx_flags``.

On GNU-like compilers, the base flags are ``-fPIC -pthread``. On
MSVC the default flags are ``/EHsc /nologo /permissive-`` for C++ and ``/nologo
/permissive-`` for C.

These defaults may be changed by providing values for three different options.
The ``base_flags`` value is always output, regardless of language. Flags
exclusive to C are specified in ``base_c_flags``, and those exclusively for
C++ should be in ``base_cxx_flags``. Note that the language-specific values are
independent from ``base_flags``; that is, providing ``base_c_flags`` or
``base_cxx_flags`` does not override or prevent the inclusion of the
``base_flags`` value, and vice-versa. Empty values are acceptable, should you
need to simply prohibit one or more of the defaults from being used.

For example, if you set ``flags`` to ``-ansi`` on a GNU-like compiler, the
resulting command line will contain ``-fPIC -pthread -ansi``. If, additionally,
you set ``base_flags`` to ``-fno-builtin`` and ``base_cxx_flags`` to
``-fno-exceptions``, the generated command will include ``-fno-builtin
-fno-exceptions -ansi`` for C++ and ``-fno-builtin -ansi`` for C.
