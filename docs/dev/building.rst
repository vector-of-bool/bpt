Building ``bpt`` from Source
############################

.. note::
  This page assumes that you have read the :doc:`env` page, and that you are
  running all commands from within the Poetry-generated virtual environment.

.. _Dagon: https://github.com/vector-of-bool/dagon

``bpt`` uses `Dagon`_ as its task execution engine. The command ``dagon`` can be
run in the root of the repository to access and execute various CI tasks. Dagon
will take care of task ordering and dependency execution.

.. note::

  By default, Dagon folds away the output of all non-failing subprocesses. To
  see more information from Dagon, pass the ``-ui=simple`` argument when running
  tasks.


Running a Build *Only*
**********************

If you only with to obtain an optimized build of the main ``bpt`` executable,
run the ``build.main`` task::

  $ dagon build.main

This will skip the audit-build and testing phases of CI and generate an
optimized ``bpt`` executable.


Rapid Iterations for Development
********************************

If you are making frequent changes to ``bpt``'s source code and want a fast
development process, run the ``build.test`` task::

  $ dagon build.test

This will build an executable with maximum debug and audit information,
including AddressSanitizer and UndefinedBehaviorSanitizer. This will also
execute the unit tests, which should run completely in under two seconds (if
they are slower, then it may be a bug).

.. note::
  While ``build.main`` writes a ``bpt`` executable directly into ``_build/``,
  ``build.test`` generates the build in a subdirectory ``_build/for-test/``.
  The differing paths allow both executables to be built simultaneously.


Toolchain Control
*****************

``dagon build.{main,test}`` will automatically select and build with an
appropriate :doc:`toolchain </guide/toolchains>` based on what it detects of the
host platform, but you may want to tweak those options.

The Dagon tasks accept two toolchain options:

``--opt=main-toolchain=>filepath>``
  This is the toolchain that is used to create an executable with the
  ``build.main`` task.

``--opt=test-toolchain=<filepath>``
  This is the toolchain that is used to create an auditing and debuggable
  executable of ``bpt``. This is the toolchain that is used if you run the
  ``build.test`` task.

The default toolchains are files contained within the ``tools/`` directory of
the repository. When ``dagon`` builds ``bpt``, it will print the path to the
toolchain file that is selected for that build.

While these provided toolchains will work perfectly well in CI, you may need to
tweak these for your build setup. For example: ``gcc-10-*.jsonc`` toolchains
assume that the GCC 10 executables are named ``gcc-10`` and ``g++-10``, which is
incorrect on some Unix and Linux distributions.

It is recommended to tweak these files as necessary to get the build working on
your system. However, **do not** include those tweaks in a commit unless they
are necessary to get the build running in CI.


What's Happening?
*****************

The ``dagon build.{main,test}`` tasks performs the following actions, in order:

#. If running the ``clean`` task, remove any prior build output and downloaded
   dependencies.
#. Prepare the prior version of ``bpt`` that will build the current version
   (usually, just download it). This is placed in ``_prebuilt/``.
#. Import the repository data from ``repo-1.dds.pizza`` into a catalog database
   stored within ``_prebuilt/``. This will be used to resolve the third-party packages that ``bpt`` itself uses.
#. Invoke the build of ``bpt`` using the prebuilt ``bpt`` obtained from the
   prior bootstrap phase.


Unit Tests
**********

Various pieces of ``bpt`` contain unit tests. These are stored within the
``src/`` directory itself in ``*.test.cpp`` files. They are built and executed
as part of the iteration cycle *unconditionally*. These tests execute in
milliseconds so as not to burden the development iteration cycle. The more
rigorous tests are executed separately by PyTest.


Speeding Up the Build
*********************

``bpt``'s build is unfortunately demanding, but can be sped up by additional
tools:


Use the LLVM ``lld`` Linker
===========================

Installing the LLVM ``lld`` linker will *significantly* improve the time it
takes for ``bpt`` and its unit test executables to link. The Dagon tasks will
automatically recognize the presence of ``lld`` if it has been installed
properly.

.. note::
  Dagon (and GCC) look for an executable called ``ld.ldd`` on the executable
  PATH (no version suffix!). You may need to symlink the version-suffixed
  executable with ``ld.ldd`` in another location on PATH so that and GCC can
  find it.


Use ``ccache``
==============

The Dagon tasks will also recognize ``ccache`` and add it as a compiler-launcher
if it is installed on your PATH. This won't improve initial compilation times,
but can make subsequent compilations significantly faster when files are
unchanged.
