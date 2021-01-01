.. highlight:: cmake

Easy Mode: Using ``dds`` in a CMake Project
###########################################

One of ``dds``'s primary goals is to inter-operate with other build systems
cleanly. Because of CMakes ubiquity, ``dds`` includes built-in support for
emitting files that can be imported into CMake.

.. seealso::

  Before reading this page, be sure to read the :ref:`build-deps.gen-libman`
  section of the :doc:`build-deps` page, which will discuss how to use the
  ``dds build-deps`` subcommand.

.. note::

  We'll first look as *easy mode*, but there's also an *easiest mode* for a
  one-line solution: :ref:`see below <cmake.pmm>`.

.. _PMM: https://github.com/vector-of-bool/PMM


Generating a CMake Import File
******************************

``build-deps`` accepts an ``--lmi-path`` argument, but also accepts a
``--cmake=<path>`` argument that serves a similar purpose: It will write a CMake
file to ``<path>`` that can be ``include()``'d into a CMake project:

.. code-block:: bash

  $ dds build-deps "neo-sqlite3^0.2.0" --cmake=deps.cmake

This will write a file ``./deps.cmake`` that we can ``include()`` from a CMake
project, which will then expose the ``neo-sqlite3`` package as a set of imported
targets.


Using the CMake Import File
===========================

Once we have generated the CMake import file using ``dds build-deps``, we can
simply import it in our ``CMakeLists.txt``::

  include(deps.cmake)

Like with ``dds``, CMake wants us to explicitly declare how our build targets
*use* other libraries. When we ``include()`` the generated CMake file, it will
generate ``IMPORTED`` targets that can be linked against.

In ``dds`` (and in libman), a library is identified by a combination of
*namespace* and *name*, joined together with a slash ``/`` character. This
*qualified name* of a library is decided by the original package author, and
should be documented. In the case of ``neo-sqlite3``, the only library is
``neo/sqlite3``.

When the generated import file imports a library, it creates a qualified name
using a double-colon "``::``" instead of a slash. As such, our ``neo/sqlite3``
is imported in CMake as ``neo::sqlite3``. We can link against it as we would
with any other target::

  add_executable(my-application app.cpp)
  target_link_libraries(my-application PRIVATE neo::sqlite3)


.. _cmake.pmm:

*Easiest* Mode: PMM Support
***************************

`PMM`_ is the *package package manager*, and can be used to control and access
package managers from within CMake scripts. This includes controlling ``dds``.
With PMM, we can automate all of the previous steps into a single line.

Refer to the ``README.md`` file in `the PMM repo <PMM>`_ for information on how
to get PMM into your CMake project. In short, download and place the
``pmm.cmake`` file in your repository, and ``include()`` the file near the top
of your ``CMakeLists.txt``::

  include(pmm.cmake)

The ``pmm()`` function also supports ``dds`` directly, and will automatically
download a prebuilt ``dds`` for the host platform and invoke ``dds build-deps``
in a single pass as part of CMake's configure process. This is especially useful
for a CI environment where you want to have a stable ``dds`` version and always
have your dependencies obtained just-in-time.

To start, pass the ``DDS`` argument to ``pmm()`` to use it::

  pmm(DDS)

.. note::
  The ``_deps`` directory and generated CMake imports file will be placed in
  the CMake build directory, out of the way of the rest of the project.

.. note::
  The version of ``dds`` that PMM downloads depends on the version of PMM
  that is in use.

This alone won't do anything useful, because you'll need to tell it what
dependencies we want to install::

  pmm(DDS DEP_FILES dependencies.json5)

You can also list your dependencies as inline strings in your CMakeLists.txt
instead of a separate file::

  pmm(DDS DEPENDS neo-sqlite3^0.2.2)

This invocation will run ``build-deps`` with the build options, generate a CMake
imports file, and immediately ``include()`` it to import the generated CMake
targets. ``pmm(DDS)`` will also generate a ``dds`` :doc:`toolchain <toolchains>`
based on the current CMake build environment, ensuring that the generated
packages have matching build options to the rest of the project. Refer to the
PMM README for more details.

.. code-block::
  :caption: ``CMakeLists.txt``
  :linenos:
  :emphasize-lines: 4,5

  cmake_minimum_required(VERSION 3.15)
  project(MyApplication VERSION 1.0.0)

  include(pmm.cmake)
  pmm(DDS DEPENDS neo-sqlite3^0.2.2)

  add_executable(my-application app.cpp)
  target_link_libraries(my-application PRIVATE neo::sqlite3)

This removes the requirement that we write a separate dependencies file, and we
no longer need to invoke ``dds build-deps`` externally, as it is all handled
by ``pmm()``.
