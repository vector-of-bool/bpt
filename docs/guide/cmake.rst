.. highlight:: cmake

Using ``bpt`` in a CMake Project
################################

One of ``bpt``'s primary goals is to inter-operate with other build systems
cleanly. Because of CMakes ubiquity, ``bpt`` includes built-in support for
emitting files that can be imported into CMake.

.. seealso::

  Before reading this page, be sure to read the :ref:`build-deps.gen-libman`
  section of the :doc:`build-deps` page, which will discuss how to use the
  ``bpt build-deps`` subcommand.

.. seealso::

  This page presents an involved and detailed process for importing
  dependencies, but there's also an *easy mode* for a one-line solution. See:
  :doc:`/howto/cmake`.

.. _PMM: https://github.com/vector-of-bool/PMM


Generating a CMake Import File
******************************

``build-deps`` accepts an ``--lmi-path`` argument, but also accepts a
``--cmake=<path>`` argument that serves a similar purpose: It will write a CMake
file to ``<path>`` that can be ``include()``'d into a CMake project:

.. code-block:: bash

  $ bpt build-deps "neo-sqlite3^0.2.0" --cmake=deps.cmake

This will write a file ``./deps.cmake`` that we can ``include()`` from a CMake
project, which will then expose the ``neo-sqlite3`` package as a set of imported
targets.


Using the CMake Import File
===========================

Once we have generated the CMake import file using ``bpt build-deps``, we can
simply import it in our ``CMakeLists.txt``::

  include(deps.cmake)

Like with ``bpt``, CMake wants us to explicitly declare how our build targets
*use* other libraries. When we ``include()`` the generated CMake file, it will
generate ``IMPORTED`` targets that can be linked against.

In ``bpt`` (and in libman), a library is identified by a combination of
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

*Easy* Mode: PMM Support
************************

`PMM`_ is the *package package manager*, and can be used to control and access
package managers from within CMake scripts. This includes controlling ``bpt``.
With PMM, we can automate all of the previous steps into a single line.

For a complete rundown on using PMM to get dependencies via ``bpt``, refer to
the :doc:`/howto/cmake` page.

Using PMM removes the requirement that we write a separate dependencies file,
and we no longer need to invoke ``bpt build-deps`` externally, as it is all
handled by PMM.
