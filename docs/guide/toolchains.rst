.. highlight:: yaml

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

In ``dds``, the default format of a toolchain is that of a single file that
describes the entire toolchain, and uses the extension ``.tc.dds`` by
convention. When running a build for a project, the ``dds`` executable will
look for a file named ``toolchain.tc.dds`` by default, and will error out if
this file does not exist. A different toolchain can be provided by passing the
toolchain file for the ``--toolchain`` (or ``-t``) option on the command line::

    $ dds build -t my-toolchain.tc.dds

Alternatively, you can pass the name of a built-in toolchain. See below.


.. _toolchains.builtin:

Built-in Toolchains
*******************

For convenience, ``dds`` includes several built-in toolchains that can be
accessed in the ``--toolchain`` command-line option using a colon ``:``
prefix::

    $ dds build -T :gcc

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


Toolchain Definitions
*********************

Besides using the built-in toolchains, it is likely that you'll soon want to
customize a toolchain further. Further customization must be done with a
file that contains the toolchain definition. The most basic toolchain file is
simply one line:

.. code-block::

    Compiler-ID: <compiler-id>

where ``<compiler-id>`` is one of the known ``Compiler-ID`` options (See the
toolchain option reference). ``dds`` will infer common suitable defaults for
the remaining options based on the value of ``Compiler-ID``.

For example, if you specify ``GNU``, then ``dds`` will assume ``gcc`` to be the
C compiler, ``g++`` to be the C++ compiler, and ``ar`` to be the library
archiving tool.

If you know that your compiler executable has a different name, you can
specify them with additional options:

.. code-block::

    Compiler-ID: GNU
    C-Compiler: gcc-9
    C++-Compiler: g++-9

``dds`` will continue to infer other options based on the ``Compiler-ID``, but
will use the provided executable names when compiling files for the respective
languages.

To specify compilation flags, the ``Flags`` option can be used:

.. code-block::

    Flags: -fsanitize=address -fno-inline

.. note::
    Use ``Warning-Flags`` to specify options regarding compiler warnings.

Flags for linking executables can be specified with ``Link-Flags``:

.. code-block::

    Link-Flags: -fsanitize=address -fPIE


.. _toolchains.opt-ref:

Toolchain Option Reference
**************************

The following options are available to be specified within a toolchain file:


``Compiler-ID``
---------------

Specify the identity of the compiler. This option is used to infer many other
facts about the toolchain. If specifying the full toolchain with the command
templates, this option is not required.

Valid values are:

``GNU``
    For GCC

``Clang``
    For LLVM/Clang

``MSVC``
    For Microsoft Visual C++


``C-Compiler`` and ``C++-Compiler``
-----------------------------------

Names/paths of the C and C++ compilers, respectively. Defaults will be inferred
from ``Compiler-ID``.


``C-Version`` and ``C++-Version``
---------------------------------

Specify the language versions for C and C++, respectively. By default, ``dds``
will not set any language version. Using this option requires that the
``Compiler-ID`` be specified

Valid ``C-Version`` values are:

- ``C89``
- ``C99``
- ``C11``
- ``C18``

Valid ``C++-Version`` values are:

- ``C++98``
- ``C++03``
- ``C++11``
- ``C++14``
- ``C++17``
- ``C++20``

.. warning::
    ``dds`` will not do any "smarts" to infer the exact option to pass to have
    the required effect. If you ask for ``C++20`` from ``gcc 5.3``, ``dds``
    will simply pass ``-std=c++20`` with no questions asked. If you need
    finer-grained control, use the ``Flags`` option.


``Warning-Flags``
-----------------

Override the compiler flags that should be used to enable warnings. This option
is stored separately from ``Flags``, as these options may be enabled/disabled
separately depending on how ``dds`` is invoked.

.. note::
    If ``Compiler-ID`` is provided, a default value will be used that enables
    common warning levels.

    If you need to tweak warnings further, use this option.


``Flags``, ``C-Flags``, and ``C++-Flags``
-----------------------------------------

Specify *additional* compiler options, possibly per-language.


``Link-Flags``
--------------

Specify *additional* link options to use when linking executables.


``Optimize``
------------

Boolean option (``True`` or ``False``) to enable/disable optimizations. Default
is ``False``.


``Debug``
---------

Boolean option (``True`` or ``False``) to enable/disable the generation of
debugging information. Default is ``False``.


``Compiler-Launcher``
---------------------

Provide a command prefix that should be used on all compiler executions.
e.g. ``ccache``.


Advanced Options Reference
**************************

The options below are probably not good to tweak unless you *really* know what
you are doing. Their values will be inferred from ``Compiler-ID``.


``Deps-Mode``
-------------

Specify the way in which ``dds`` should track compilation dependencies. One
of ``GNU``, ``MSVC``, or ``None``.


``C-Compile-File``
------------------

Override the *command template* that is used to compile C source files.


``C++-Compile-File``
--------------------

Override the *command template* that is used to compile C++ source files.


``Create-Archive``
------------------

Override the *command template* that is used to generate static library archive
files.


``Link-Executable``
-------------------

Override the *command template* that is used to link executables.


``Include-Template``
--------------------

Override the *command template* for the flags to specify a header search path.


``External-Include-Template``
-----------------------------

Override the *command template* for the flags to specify a header search path
of an external library.


``Define-Template``
-------------------

Override the *command template* for the flags to set a preprocessor definition.
