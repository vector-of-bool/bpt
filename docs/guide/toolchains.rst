.. highlight:: js

Toolchains
##########

One of the core components of ``dds`` is that of the *toolchain*. A toolchain
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
    **IMPORTANT**: ``dds`` will *not* automatically load the Visual C++
    environment. To use Visual C++, ``dds`` must be executed from the
    appropriate environment in order for the Visual C++ toolchain executables
    and files to be available.


Passing a Toolchain
*******************

In ``dds``, the default format of a toolchain is that of a single JSON5 file
that describes the entire toolchain. When running a build for a project, the
``dds`` executable will look in a few locations for a default toolchain, and
generate an error if no default toolchain file is found (Refer to
:ref:`toolchains.default`). A different toolchain can be provided by passing
the toolchain file for the ``--toolchain`` (or ``-t``) option on the command
line::

    $ dds build -t my-toolchain.json5

Alternatively, you can pass the name of a built-in toolchain. See below.


.. _toolchains.builtin:

Built-in Toolchains
*******************

For convenience, ``dds`` includes several built-in toolchains that can be
accessed in the ``--toolchain`` command-line option using a colon ``:``
prefix::

    $ dds build -t :gcc

``dds`` will treat the leading colon (``:``) as a name for a built-in
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

If you do not which to provide a new toolchain for every individual project,
and the built-in toolchains do not suit your needs, you can write a toolchain
file to one of a few predefined paths, and ``dds`` will find and use it for the
build. The following directories are searched, in order:

#. ``$pwd/`` - If the working directory contains a toolchain file, it will be
   used as the default.
#. ``$dds_config_dir/`` - Searches for a toolchain file in ``dds``'s user-local
   configuration directory (see below).
#. ``$user_home/`` - Searches for a toolchain file at the root of the current
   user's home directory. (``$HOME`` on Unix-like systems, and ``$PROFILE`` on
   Windows.)

In each directory, it will search for ``toolchain.json5``, ``toolchain.jsonc``,
or ``toolchain.json``.

The ``$dds_user_config`` directory is the ``dds`` subdirectory of the
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
toolchain option reference). ``dds`` will infer common suitable defaults for
the remaining options based on the value of ``compiler_id``.

For example, if you specify ``gnu``, then ``dds`` will assume ``gcc`` to be the
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

``dds`` will continue to infer other options based on the ``compiler_id``, but
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

Many of the ``dds`` toolchain parameters accept argument lists or shell-string
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

Despite splitting strings as-if they were shell commands, ``dds`` does nothing
else shell-like. It does not expand environment variables, nor does it expand
globs.


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

Specify the language versions for C and C++, respectively. By default, ``dds``
will not set any language version. Using this option requires that the
``compiler_id`` be specified. Setting this value will cause the corresponding
language-version flag to be passed to the compiler.

Valid ``c_version`` values are:

- ``c89``
- ``c99``
- ``c11``
- ``c18``

Valid ``cxx_version`` values are:

- ``c++98``
- ``c++03``
- ``c++11``
- ``c++14``
- ``c++17``
- ``c++20``

.. warning::
    ``dds`` will not do any "smarts" to infer the exact option to pass to have
    the required effect. If you ask for ``c++20`` from ``gcc 4.8``, ``dds``
    will simply pass ``-std=c++20`` with no questions asked. If you need
    finer-grained control, use the ``c_flags`` and ``cxx_flags`` options.


``warning_flags``
-----------------

Override the compiler flags that should be used to enable warnings. This option
is stored separately from ``flags``, as these options may be enabled/disabled
separately depending on how ``dds`` is invoked.

.. note::
    If ``compiler_id`` is provided, a default value will be used that enables
    common warning levels.

    If you need to tweak warnings further, use this option.

On GNU-like compilers, the default flags are ``-Wall -Wextra -Wpedantic
-Wconversion``. On MSVC the default flag is ``/W4``.


``flags``, ``c_flags``, and ``cxx_flags``
-----------------------------------------

Specify *additional* compiler options, possibly per-language.


``link_flags``
--------------

Specify *additional* link options to use when linking executables.


``optimize``
------------

Boolean option (``true`` or ``false``) to enable/disable optimizations. Default
is ``false``.


``debug``
---------

Boolean option (``true`` or ``false``) to enable/disable the generation of
debugging information. Default is ``false``.


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
templates from which ``dds`` will create a command-line for a subprocess,
possibly by combining them together.

Each command template allows some set of placeholders. Each instance of the
placeholder string will be replaced in the final command line. Refer to each
respective option for more information.


``deps_mode``
-------------

Specify the way in which ``dds`` should track compilation dependencies. One
of ``gnu``, ``msvc``, or ``none``.

.. note::
    If ``none``, then dependency tracking will be disabled entirely. This will
    prevent ``dds`` from tracking interdependencies of source files, and
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
        c_compile_file:   "<compiler> -fPIC -pthread [flags] -c [in] -o[out]",
        cxx_compile_file: "<compiler> -fPIC -pthread [flags] -c [in] -o[out]",
        // When `optimize` is enabled, `-O2` is added as a flag
        // When `debug` is enabled, `-g` is added as a flag

        // On MSVC:
        c_compile_file:   "cl.exe /MT /nologo /permissive- [flags] /c [in] /Fo[out]",
        cxx_compile_file: "cl.exe /MT /EHsc /nologo /permissive- [flags] /c [in] /Fo[out]",
        // When `optimize` is enabled, `/O2` is added as a flag
        // When `debug` is enabled, `/Z7` and `/DEBUG` are added, and `/MT` becomes `/MTd`
    }


``create_archive``
------------------

Override the *command template* that is used to generate static library archive
files.

This template expects three placeholders:

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


``tty_flags``
-------------

Supply additional flags when compiling/linking that will only be applied if
standard output is an ANSI-capable terminal.

On GNU and Clang this will be ``-fdiagnostics-color`` by default.


``obj_prefix``, ``obj_suffix``, ``archive_prefix``, ``archive_suffix``,
``exe_prefix``, and ``exe_suffix``
----------------------------------

Set the filename prefixes and suffixes for object files, library archive files,
and executable files, respectively.
