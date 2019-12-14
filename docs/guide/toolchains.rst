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
