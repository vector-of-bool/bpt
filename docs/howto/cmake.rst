How Do I Use |bpt| in a CMake Project?
########################################

.. highlight:: cmake

If you have a CMake project and you wish to pull your dependencies via |bpt|,
you're in luck: Such a process is explicitly supported. Here's the recommended
approach:

#. Download `PMM`_ and place and commit `the PMM script <pmm.cmake>`_ into your
   CMake project. [#f1]_
#. In your ``CMakeLists.txt``, ``include()`` ``pmm.cmake``.
#. Call ``pmm(BPT)`` and list your dependencies.

Below, we'll walk through this in more detail.

.. note::

  You don't even have to have |bpt| downloaded and present on your system to
  use |bpt| in PMM! Read on...


Using PMM
*********

`PMM`_ is the *Package Manager Manager* for CMake, and is designed to offer
greater integration between a CMake build and an external package management
tool. `PMM`_ supports Conan, vcpkg, and, of course, |bpt|.

.. seealso::

  Refer to the ``README.md`` file in `the PMM repo <PMM>`_ for information on
  how to use PMM.


Getting PMM
===========

To use PMM, you need to download one only file and commit it to your project:
`pmm.cmake`_, the entrypoint for PMM [#f1]_. It is not significant where the
``pmm.cmake`` script is placed, but it should be noted for inclusion.

``pmm.cmake`` should be committed to the project because it contains version
pinning settings for PMM and can be customized on a per-project basis to alter
its behavior for a particular project's needs.


Including PMM
=============

Suppose I have downloaded and committed `pmm.cmake`_ into the ``tools/``
subdirectory of my CMake project. To use it in CMake, I first need to
``include()`` the script. The simplest way is to simply ``include()`` the file

.. code-block::
  :caption: CMakeLists.txt
  :emphasize-lines: 4

  cmake_minimum_required(VERSION 3.12)
  project(MyApplication VERSION 2.1.3)

  include(tools/pmm.cmake)

The ``include()`` command should specify the path to ``pmm.cmake``, including
the file extension, relative to the directory that contains the CMake script
that contains the ``include()`` command.


Running PMM
===========

Simply ``include()``-ing PMM won't do much, because we need to actually *invoke
it*.

PMM's main CMake command is ``pmm()``. It takes a variety of options and
arguments for the package managers it supports, but we'll only focus on |bpt|
for now.

The basic signature of the ``pmm(BPT)`` command looks like this::

  pmm(BPT [DEP_FILES [filepaths...]]
          [DEPENDS [dependencies...]]
          [TOOLCHAIN file-or-id])

The most straightforward usage is to use only the ``DEPENDS`` argument. For
example, if we want to import `{fmt} <https://fmt.dev>`_::

  pmm(BPT DEPENDS "fmt^7.0.3")

When CMake executes the ``pmm(BPT ...)`` line above, PMM will download the
appropriate |bpt| executable for your platform, generate
:doc:`a bpt toolchain </guide/toolchains>` based on the CMake environment, and
then invoke ``bpt build-deps`` to build the dependencies that were listed in the
``pmm()`` invocation. The results from ``build-deps`` are then imported into
CMake as ``IMPORTED`` targets that can be used by the containing CMake project.

.. seealso::

  For more in-depth discussion on ``bpt build-deps``, refer to
  :doc:`/guide/build-deps`.

.. note::
  The ``_deps`` directory and generated CMake imports file will be placed in
  the CMake build directory, out of the way of the rest of the project.

.. note::
  The version of |bpt| that PMM downloads depends on the version of PMM
  that is in use.


Using the ``IMPORTED`` Targets
==============================

Like with |bpt|, CMake wants us to explicitly declare how our build targets
*use* other libraries. After ``pmm(BPT)`` executes, there will be ``IMPORTED``
targets that can be linked against.

In |bpt| (and in libman), a library is identified by a combination of
*namespace* and *name*, joined together with a slash ``/`` character. This
*qualified name* of a library is decided by the original package author or
maintainer, and should be documented. In the case of ``fmt``, the only library
is ``fmt/fmt``.

When ``pmm(BPT)`` imports a library, it creates a qualified name using a
double-colon "``::``" instead of a slash. As such, our ``fmt/fmt`` is imported
in CMake as ``fmt::fmt``. We can link against it as we would with any other
target::

  add_executable(my-application app.cpp)
  target_link_libraries(my-application PRIVATE fmt::fmt)

This will allow us to use **{fmt}** in our CMake project as an external
dependency.

In all, this is our final ``CMakeLists.txt``:

.. code-block::
  :caption: ``CMakeLists.txt``

  cmake_minimum_required(VERSION 3.12)
  project(MYApplication VERSION 2.1.3)

  include(tools/pmm.cmake)
  pmm(BPT DEPENDS fmt^7.0.3)

  add_executable(my-application app.cpp)
  target_link_libraries(my-application PRIVATE fmt::fmt)


Changing Compile Options
************************

|bpt| supports setting compilation options using
:doc:`toolchains </guide/toolchains>`. PMM supports specifying a toolchain using
the ``TOOLCHAIN`` argument::

  pmm(BPT DEPENDS fmt^7.0.3 TOOLCHAIN my-toolchain.json5)

Of course, writing a separate toolchain file just for your dependencies can be
tedious. For this reason, PMM will write a toolchain file on-the-fly when it
executes |bpt|. The generated toolchain is created based on the current CMake
settings when ``pmm()`` was executed.

To add compile options, simply ``add_compile_options``::

  add_compile_options(-fsanitize=address)
  pmm(BPT ...)

The above will cause all |bpt|-built dependencies to compile with
``-fsanitize=address`` as a command-line option.

The following CMake variables and directory properties are used to generate the
|bpt| toolchain:

``COMPILE_OPTIONS``
  Adds additional compiler options. Should be provided by
  ``add_compile_options``.

``COMPILE_DEFINITIONS``
  Add preprocessor definitions. Should be provided by
  ``add_compile_definitions``

``CXX_STANDARD``
  Control the ``cxx_version`` in the toolchain

``CMAKE_MSVC_RUNTIME_LIBRARY``
  Sets the ``runtime`` option. This option has limited support for generator
  expressions.

``CMAKE_C_FLAGS`` and ``CMAKE_CXX_FLAGS``, and their per-config variants
  Set the basic compile flags for the respective file types

``CXX_COMPILE_LAUNCHER``
  Allow providing a compiler launcher, e.g. ``ccache``.

.. note::

  Calls to ``add_compile_options``, ``add_compile_definitions``, or other CMake
  settings should appear *before* calling ``pmm(BPT)``, since the toolchain file
  is generated and dependencies are built at that point.

  ``add_link_options`` has no effect on the |bpt| toolchain, as |bpt| does
  not generate any runtime binaries.

.. rubric:: Footnotes

.. [#f1]
  Do not use ``file(DOWNLOAD)`` to "automatically" obtain `pmm.cmake`_. The
  ``pmm.cmake`` script is already built to do this for the rest of PMM. The
  `pmm.cmake`_ script itself is very small and is *designed* to be copy-pasted
  and committed into other projects.

.. _PMM: https://github.com/vector-of-bool/pmm
.. _pmm.cmake: https://github.com/vector-of-bool/pmm/raw/master/pmm.cmake
