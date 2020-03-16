Packages and Layout
###################

The units of distribution in ``dds`` are *packages*. A single package consists
of one or more *libraries*. In the simplest case, a package will contain a
single library.

It may be easiest to work from the bottom-up when trying to understand how
``dds`` understands code.

The layout expected by ``dds`` is based on the `Pitchfork layout`_ (PFL).
``dds`` does not make use of every provision of the layout document, but the
features it does have are based on PFL.

.. _Pitchfork layout: https://api.csswg.org/bikeshed/?force=1&url=https://raw.githubusercontent.com/vector-of-bool/pitchfork/develop/data/spec.bs

In particular, the following directories are used:

- ``src/``
- ``include/``
- ``libs/``
- ``_build/`` (the default build output directory used by ``dds``).

Note that the ``libs/*/`` directories can contain their own ``src/`` and
``include/`` directories, the purposes and behaviors of which match those of
their top-level counterparts.


Source Files
************

The smallest subdivision of code that ``dds`` recognizes is the *source file*,
which is exactly as it sounds: A single file containing some amount of code.

Source files can be grouped on a few axes, the most fundamental of which is
"Is this compiled?"

``dds`` uses source file extensions to determine whether a source file should
be fed to the compiler. All of the common C and C++ file extensions are
supported:

.. list-table::

    - * Compiled as C
      * ``.c`` and ``.C``

    - * Compiled as C++
      * ``.cpp``, ``.c++``, ``.cc``, and ``.cxx``

    - * Not compiled
      * ``.H``, ``.H++``, ``.h``, ``.h++``, ``.hh``, ``.hpp``, ``.hxx``, and ``.inl``

If a file's extension is not listed in the table above, ``dds`` will ignore it.

.. note::
    Although headers are not compiled, this does not mean they are ignored.
    ``dds`` still understands and respects headers, and they are collected
    together as part of *source distribution*.


.. _pkgs.apps-tests:

Applications and Tests
**********************

``dds`` will recognize certain compilable source files as belonging to
applications and tests. If a compilable source file stem ends with ``.main`` or
``.test``, that source file is assumed to correspond to an executable to
generate. The filename stem before the ``.main`` or ``.test`` will be used as
the name of the generated executable. For example:

- ``foo.main.cpp`` will generate an executable named ``foo``.
- ``bar.test.cpp`` will generate an executable named ``bar``.
- ``cat-meow.main.cpp`` will generate an executable named ``cat-meow``.
- ``cats.musical.test.cpp`` will generate an executable named ``cats.musical``.

.. note::
    ``dds`` will automatically append the appropriate filename extension to the
    generated executables based on the host and toolchain.

An *application* source file is a source file whose file stem ends with
``.main``. ``dds`` will assume this source file to contain a program entry
point function and not include it as part of the main library build. Instead,
when ``dds`` is generating applications, the source file will be compiled, and
the resulting object will be linked together with the enclosing library into an
executable.

A *test* source file is a source file whose file stem ends with ``.test``. Like
application sources, a *test* source file is omitted from the main library, and
it will be used to generate tests. The exact behavior of tests is determined by
the ``test_driver`` setting for the package, but the default is that each test
source file will generate a single test executable that is executed by ``dds``
when running unit tests.

The building of tests and applications can be controlled when running
``dds build``. If tests are built, ``dds`` will automatically execute those
tests in parallel once the executables have been generated.

In any case, the executables are associated with a *library*, and, when those
executables are linked, the associated library (and its dependencies) will be
linked into the final executable. There is no need to manually specify this
linking behavior.


.. _pkg.source-root:

Source Roots
************

Source files are collected as children of some *source root*. A *source
root* is a single directory that contains some *portable* bundle of source
files. The word "portable" is important: It is what distinguishes the
source root from its child directories.


Portability
===========

By saying that a source root is "portable",  It indicates that the directory
itself can be moved, renamed, or copied without breaking the ``#include``
directives of its children or of the directory's referrers.

As a practical example, let's examine such a directory, which we'll call
``src/`` for the purposes of this example. Suppose we have a directory named
``src`` with the following structure:

.. code-block:: text

    <path>/src/
      animals/
        mammal/
          mammal.hpp
        cat/
          cat.hpp
          sound.hpp
          sound.cpp
        dog/
          dog.hpp
          sound.hpp
          sound.cpp

In this example, ``src/`` is a *source root*, but ``src/animals/``,
``src/animals/cat/``, and ``src/animals/dog/`` are **not** source roots.
While they may be directories that contain source files, they are not "roots."

Suppose now that ``dog.hpp`` contains an ``#include`` directive:

.. code-block:: c++

    #include <animals/mammal/mammal.hpp>

or even a third-party user that wants to use our library:

.. code-block:: c++

    #include <animals/dog/dog.hpp>
    #include <animals/dog/sound.hpp>

In order for any code to compile and resolve these ``#include`` directives, the
``src/`` directory must be added to their *include search path*.

Because the ``#include`` directives are based on the *portable* source root,
the exactly location of ``src/`` is not important to the content of the
consuming source code, and can thus be relocated and renamed as necessary.
Consumers only need to update the path of the *include search path* in a single
location rather than modifying their source code.


.. _pkgs.source-root:

Source Roots in ``dds``
=======================

To avoid ambiguity and aide in portability, the following rules should be
strictly adhered to:

#. Source roots may not contain other source roots.
#. Only source roots will be added to the *include-search-path*.
#. All ``#include``-directives are relative to a source root.

By construction, ``dds`` cannot build a project that has nested source roots,
and it will only ever add source roots to the *include-search-path*.

``dds`` supports either one or two source roots in a library.


.. _pkgs.lib-roots:

Library Roots
*************

In ``dds``, a *library root* is a directory that contains a ``src/`` directory,
an ``include/`` directory, or both. ``dds`` will treat both directories as
source roots, but behaves differently between the two. The ``src/`` and
``include/`` directories are themselves *source roots*.

``dds`` distinguishes between a *public* include-directory, and a *private*
include-directory. When ``dds`` is compiling a library, both its *private* and
its *public* include-paths will be added to the compiler's
*include-search-path*. When a downstream user of a library is compiling against
a library managed by ``dds``, only the *public* include-directory will be
added to the compiler's *include-search-path*. This has the effect that only
the files that are children of the source root that is the *public*
include-directory will be available when compiling consumers.

.. warning::
    Because only the *public* include-directory is available when compiling
    consumers, it is essential that no headers within the *public*
    include-directory attempt to use headers from the *private*
    include-directory, as they **will not** be visible.

If both ``src/`` and ``include/`` are present in a library root, then ``dds``
will use ``include/`` as the *public* include-directory and ``src/`` as the
*private* include-directory. If only one of the two is present, then that
directory will be treated as the *public* include-directory, and there will be
no *private* include-directory.

When ``dds`` exports a library, the header files from the *public*
include-directory source root will be collected together and distributed as
that library's header tree. The path to the individual header files relative to
their source root will be retained as part of the library distribution.

``dds`` will compile every compilable source file that appears in the ``src/``
directory. ``dds`` will not compile compilable source files that appear in the
``include/`` directory and will issue a warning on each file found.


.. _pkgs.libs:

Libraries
*********

The *library* is a fundamental unit of consumable code, and ``dds`` is
specifically built to work with them. When you are in ``dds``, the library is
the center of everything.

A single *library root* will always correspond to exactly one library. If the
library has any compilable sources then ``dds`` will use those sources to
generate a static library file that is linked into runtime binaries. If a
library contains only headers then ``dds`` will not generate an archive to be
included in downstream binaries, but it will still generate link rules for the
dependencies of a header-only library.

In order for ``dds`` to be able to distribute and interlink libraries, a
``library.json5`` file must be present at the corresponding library root. The
only required key in a ``library.json5`` file is ``name``:

.. code-block:: js

  {
    name: 'my-library'
  }

.. seealso:: More information is discussed on the :ref:`deps.lib-deps` page


.. _pkgs.pkg-root:

Package Roots
*************

A *package root* is a directory that contains some number of library roots. If
the package root contains a ``src/`` and/or ``include/`` directory then the
package root is itself a library root, and a library is defined at the root of
the package. This is intended to be the most common and simplest method of
creating libraries with ``dds``.

If the package root contains a ``libs/`` directory, then each subdirectory of
the ``libs/`` directory is checked to be a library root. Each direct child of
the ``libs/`` directory that is also a library root is added as a child of the
owning package.


.. _pkgs.pkgs:

Packages
********

A package is defined by some *package root*, and contains some number of
*libraries*.

The primary distribution format of packages that is used by ``dds`` is the
*source distribution*. Refer to the page :doc:`source-dists`.

Packages are identified by a name/version pair, joined together by an ``@``
symbol. The version of a package must be a semantic version string. Together,
the ``name@version`` string forms the *package ID*, and it must be unique
within a repository or package catalog.

In order for a package to be exported by ``dds`` it must have a
``package.json5`` file at its package root. Three keys are required to be
present in the ``package.json5`` file: ``name``, ``version``, and ``namespace``:

.. code-block:: js

    {
      name: 'acme-widgets',
      version: '6.7.3',
      namespace: 'acme',
    }

``version`` must be a valid semantic version string.

.. note::
  The ``namespace`` key is arbitrary, and not necessarily associated with
  any C++ ``namespace``.

.. seealso::
  The purpose of ``namespace``, as well as additional options in this file,
  are described in the :ref:`deps.pkg-deps` page


.. _pkgs.naming-reqs:

Naming Requirements
===================

Package names aren't a complete free-for-all. Package names must follow a set
of specific rules:

- Package names may consist of a subset of ASCII including lowercase
  characters, digits, underscores (``_``), hyphens (``-``), and periods
  (``.``).

  .. note::
    Different filesystems differ in their handling of filenames. Some platforms
    perform unicode and case normalization, which can significantly confuse tools
    that don't use the same normalization rules. Different platforms have
    different filename limitations and allowable characters. This set of
    characters is valid on most currently popular filesystems.

- Package names must begin with an alphabetic character
- Package names must end with an alphanumeric character (letter or digit).
- Package names may not contain adjacent punctuation characters.
