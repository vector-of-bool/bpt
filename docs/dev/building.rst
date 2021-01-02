Building ``dds`` from Source
############################

.. note::

  This page assumes that you have ready the :doc:`env` page, and that you are
  running all commands from within the Poetry-generated virtual environment.

The main entrypoint for the ``dds`` CI process is the ``dds-ci`` command, which
will build and test the ``dds`` from the repository sources. ``dds-ci`` accepts
several optional command-line arguments to tweak its behavior.


Running a Build *Only*
**********************

If you only wish to obtain a built ``dds`` executable, the ``--no-test``
parameter can be given::

  $ dds-ci --no-test

This will skip the audit-build and testing phases of CI and build only the final
``dds`` executable.


Rapid Iterations for Development
********************************

If you are making frequent changes to ``dds``'s source code and want a fast
development process, use ``--rapid``::

  $ dds-ci --rapid

This will build the build step only, and builds an executable with maximum debug
and audit information, including AddressSanitizer and
UndefinedBehaviorSanitizer. This will also execute the unit tests, which should
run completely in under two seconds (if they are slower, then it may be a bug).


Toolchain Control
*****************

``dds-ci`` will automatically select and build with an appropriate
:doc:`toolchain </guide/toolchains>` based on what it detects of the host
platform, but you may want to tweak those options.

The ``dds-ci`` script accepts two toolchain options:

``--main-toolchain``
  This is the toolchain that is used to create a final release-built executable.
  If you build with ``--no-test``, this toolchain will be used.

``--test-toolchain`` This is the toolchain that is used to create an auditing
and debuggable executable of ``dds``. This is the toolchain that is used if you
build with ``--rapid``.

If you build with neither ``--rapid`` nor ``--no-test``, then ``dds-ci`` will
build *two* ``dds`` executables: One with the ``--test-toolchain`` that is
passed through the test suite, and another for ``--main-toolchain`` that is
built for distribution.

The default toolchains are files contained within the ``tools/`` directory of
the repository. When ``dds-ci`` builds ``dds``, it will print the path to the
toolchain file that is selected for that build.

While these provided toolchains will work perfectly well in CI, you may need to
tweak these for your build setup. For example: ``gcc-9-*.jsonc`` toolchains
assume that the GCC 9 executables are named ``gcc-9`` and ``g++-9``, which is
incorrect on some Unix and Linux distributions.

It is recommended to tweak these files as necessary to get the build working on
your system. However, **do not** include those tweaks in a commit unless they
are necessary to get the build running in CI.


What's Happening?
*****************

The ``dds-ci`` script performs the following actions, in order:

#. If given ``--clean``, remove any prior build output and downloaded
   dependencies.
#. Prepare the prior version of ``dds`` that will build the current version
   (usually, just download it). This is placed in ``_prebuilt/``.
#. Import the ``old-catalog.json`` into a catalog database stored within
   ``_prebuilt/``. This will be used to resolve the third-party packages that
   ``dds`` itself uses.
#. Invoke the build of ``dds`` using the prebuilt ``dds`` obtained from the
   prior bootstrap phase. If ``--no-test`` or ``--rapid`` was specified, the CI
   script stops here.
#. Launch ``pytest`` with the generated ``dds`` executable and start the final
   release build simultaneously, and wait for both to finish.


Unit Tests
**********

Various pieces of ``dds`` contain unit tests. These are stored within the
``src/`` directory itself in ``*.test.cpp`` files. They are built and executed
as part of the iteration cycle *unconditionally*. These tests execute in
milliseconds so as not to burden the development iteration cycle. The more
rigorous tests are executed separately by PyTest.


Speeding Up the Build
*********************

``dds``'s build is unfortunately demanding, but can be sped up by additional
tools:


Use the LLVM ``lld`` Linker
===========================

Installing the LLVM ``lld`` linker will *significantly* improve the time it
takes for ``dds`` and its unit test executables to link. ``dds-ci`` will
automatically recognize the presence of ``lld`` if it has been installed
properly.

.. note::

  ``dds-ci`` (and GCC) look for an executable called ``ld.ldd`` on the
  executable PATH (no version suffix!). You may need to symlink the
  version-suffixed executable with ``ld.ldd`` in another location on PATH so
  that ``dds-ci`` (and GCC) can find it.


Use ``ccache``
==============

``dds-ci`` will also recognize ``ccache`` and add it as a compiler-launcher if
it is installed on your PATH. This won't improve initial compilation times, but
can make subsequent compilations significantly faster when files are unchanged.
