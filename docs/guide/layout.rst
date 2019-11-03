Project Layout
##############

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


.. _guide.layout.include:

Include Directories and Header Resolution
*****************************************

A compiler's "include path" is the list of directories in which it will attempt
to resolve ``#include`` directives.

The layout prescriptions permit either ``src/``, ``include/``, or both. In the
presence of both, the ``include/`` directory is used as the *public* include
directory, and ``src/`` is used as the *private* include directory. When only
one of either is present, that directory will be treated as the *public*
include directory (and there will be no *private* include directory).


.. _guide.layout.sources:

Source Files
************

``dds`` distinguishes between *headers* and *compilable* sources. The heuristic
used is based on common file extensions:

The following are considered to be *header* source files:

- ``.h``
- ``.hpp``
- ``.hxx``
- ``.inl``
- ``.h++``

While the following are considered to be *compilable* source files:

- ``.c``
- ``.cpp``
- ``.cc``
- ``.cxx``
- ``.c++``

``dds`` will compile every compilable source file that appears in the ``src/``
directory. ``dds`` will not compile compilable source files that appear in the
``include/`` directory and will issue a warning on each file found.


.. _guide.layout.apps-tests:

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

If the inner extension is ``.main``, then ``dds`` will assume the corresponding
executable to be an *application*. If the inner extension is ``.test``, ``dds``
will assume the executable to be a test.

The building of tests and applications can be controlled when running
``dds build``. If tests are built, ``dds`` will automatically execute those
tests in parallel once the executables have been generated.

In any case, the executables are associated with a *library*, and, when those
executables are linked, the associated library (and its dependencies) will be
linked into the final executable. There is no need to manually specify this
linking behavior.


.. _guide.layout.libraries:

Libraries
*********

The *library* is a fundamental unit of consumable code, and ``dds`` is
specifically built to work with them. When you are in ``dds``, the library is
the center of everything.

A *source root* is a directory that contains the ``src/`` and/or ``include/``
directories. The ``src/`` and ``include/`` directories are themselves
*source directories*. A single *source root* will always correspond to exactly
one library. If the library has any compilable sources then ``dds`` will use
those sources to generate a static library file that is linked into runtime
binaries. If a library contains only headers then ``dds`` will not generate an
archive to be included in downstream binaries, but it will still generate link
rules for the dependencies of a header-only library.

In the previous section, :ref:`guide.layout.apps-tests`, it was noted that
applications and tests are associated with a library. This association is
purely based on being collocated within the same source root.

When an executable is built within the context of a library, that library (and
all of its dependencies) will be linked into that executable.
