Building ``dds`` from Source
############################

While prebuilt ``dds`` executables are `available on the GitHub page
<releases_>`_, one may wish to build ``dds`` from source.

.. _releases: https://github.com/vector-of-bool/dds/releases

The ``dds`` build process is designed to be as turn-key simple as possible.


Platform Support
****************

``dds`` aims to be as cross-platform as possible. It currently build and
executes on Windows, macOS, Linux, and FreeBSD. Support for additional
platforms is possible but will require modifications to ``bootstrap.py`` that
will allow it to be built on such platforms.


Build Requirements
******************

Building ``dds`` has a simple set of requirements:

- **Python 3.6** or newer to run the bootstrap/CI scripts.
- A C++ compiler that has rudimentary support for C++20 concepts. Newer
  releases of Visual C++ that ship with **VS 2019** will be sufficient on
  Windows, as will **GCC 9** with ``-fconcepts`` on other platforms.

.. note::
    On Windows, you will need to execute the build from within a Visual C++
    enabled environment. This will involve launching the build from a Visual
    Studio Command Prompt.

.. note::
    At the time of writing, C++20 Concepts has not yet been released in Clang,
    but should be available in LLVM/Clang 11 and newer.


Build Scripts and the CI Process
********************************

The main CI process is driven by Python. The root CI script is ``tools/ci.py``,
and it accepts several command-line parameters. Only a few of are immediate
interest:

``--bootstrap-with=<method>`` or ``-B <method>``
    Tell ``ci.py`` how to obtain the previous ``dds`` executable that can build
    the *current* ``dds`` source tree. This accepts one of three values:
    ``skip``, ``download``, or ``build``. Refer to :ref:`bootstrapping`.

``--build-only``
    A flag that tells ``ci.py`` to exit after it has successfully built the
    current source tree, and to not execute the phase-2 build nor the automated
    tests.

``--toolchain=<path>`` or ``-T <path>``
    Tell ``ci.py`` what toolchain to give to the prior ``dds`` to build the
    current ``dds``.

The ``ci.py`` script performs the following actions, in order:

#. Prepare the build output directory
#. Bootstrap the prior version of ``dds`` that will build the current version.
#. Import the embedded ``catalog.json`` into a catalog database stored within
   ``_build/``. This will be used to resolve the third-party packages that
   ``dds`` itself uses.
#. Invoke the build of ``dds`` using the prebuilt ``dds`` from the prior
   bootstrap phase. If ``--build-only`` was specified, the CI script stops
   here.
#. Use the new ``dds`` executable to rebuild itself *again* (phase-2 self-build
   test). A bit of a "sanity test."
#. Execute the test suite using ``pytest``.


.. _bootstrapping:

Bootstrapping ``dds``
*********************

In the beginning, ``dds`` was built by a Python script that globbed the sources
and invoked the compiler+linker on those sources. Once ``dds`` was able to
build and link itself, this Python script was replaced instead with ``dds``
building itself. ``dds`` has never used another build system.

The ``ci.py`` script accepts one of three methods for the ``--bootstrap-with``
flag: ``skip``, ``download``, or ``build``.

Once bootstrapping is complete, a ``dds`` executable will be written to
``_prebuilt/dds``. This executable refers to a **previous** version of ``dds``
that is able to build the newer ``dds`` source tree.

.. note::
    For all development work on ``dds``, the ``_prebuilt/dds`` executable should
    always be used. This means that newer ``dds`` features are not available
    for use within the ``dds`` repository.


Bootstrap: ``skip``
===================

If given ``skip``, ``ci.py`` will not perform any bootstrapping steps. It will
assume that there is an existing ``_prebuilt/dds`` executable. This option
should be used once bootstrapping has been performed at least once with another
method, as this is much faster than rebuilding/redownloading every time.


Bootstrap: ``download``
=======================

The ``ci.py`` script has a reference to a download URL of the prior version of
``dds`` that has been designated for the bootstrap. These executables originate
from `the GitHub releases <releases_>`_ page.

If given ``download``, then ``ci.py`` will download a predetermined ``dds``
executable and use it to perform the remainder of the build.


Bootstrap: ``build``
====================

Another script, ``tools/bootstrap.py`` is able to build ``dds`` from the ground
up. It works by progressively cloning previous versions of the ``dds``
repository and using them to build the next commit in the chain.

While this is a neat trick, it isn't necessary for most development, as the
resulting executable will be derived from the same commit as the executable
that would be obtained using the ``download`` method. This is also more fragile
as the past commits may make certain assumptions about the system that might
not be true outside of the CI environment. The build process may be tweaked in
the future to correct these assumptions.


Selecting a Build Toolchain
***************************

``dds`` includes three toolchains that it uses to build itself in its CI
environment: ``tools/gcc-9.jsonc`` for Linux and macOS,
``tools/freebsd-gcc-9.jsonc`` for FreeBSD, and ``tools/msvc.jsonc`` for
Windows.

While these toolchains will work perfectly well in CI, you may need to tweak
these for your build setup. For example: ``gcc-9.jsonc`` assumes that the GCC 9
executables are named ``gcc-9`` and ``g++-9``, which is incorrect on some
Linux distributions.

It is recommended to tweak these files as necessary to get the build working on
your system. However, do not include those tweaks in a commit unless they are
necessary to get the build running in CI.


Giving a Toolchain to ``ci.py``
===============================

Just like passing a toolchain to ``dds``, ``ci.py`` also requires a toolchain.
Simply pass the path to your desired toolchain using the ``--toolchain``/
``-T`` argument:

.. code-block:: bash

    $ python3 tools/ci.py [...] -T tools/gcc-9.jsonc


Building for Development
************************

While ``ci.py`` is rigorous in maintaining a clean and reproducible environment,
we often don't need such rigor for a rapid development iteration cycle. Instead
we can invoke the build command directly in the same way that ``ci.py`` does
it:

.. code-block:: bash

    $ _prebuilt/dds build -t [toolchain] \
        --catalog _build/catalog.db \
        --repo-dir _build/ci-repo

The ``--catalog`` and ``--repo-dir`` arguments are not strictly necessary, but
help to isolate the ``dds`` dev environment from the user-local ``dds``
environment. This is important if modifications are made to the catalog
database schema that would conflict with the one of an external ``dds``
version.

.. note::
    You'll likely want to run ``ci.py`` *at least once* for it to prepare the
    necessary ``catalog.db``.

.. note::
    As mentioned previously, if using MSVC, the above command must execute with
    the appropriate VS development environment enabled.


Running the Test Suite
**********************

The ``--build-only`` flag for ``ci.py`` will disable test execution. When this
flag is omitted, ``ci.py`` will execute a self-build sanity test and then
execute the main test suite, which is itself written as a set of ``pytest``
tests in the ``tests/`` subdirectory.


Unit Tests
==========

Various pieces of ``dds`` contain unit tests. These are stored within the
``src/`` directory itself in ``*.test.cpp`` files. They are built and executed
by the bootstrapped ``dds`` executable unconditionally. These tests execute
in milliseconds and do not burden the development iteration cycle.
