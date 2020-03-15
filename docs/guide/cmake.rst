.. highlight:: cmake

Using ``dds`` Packages in a CMake Project
#########################################

One of ``dds``'s primary goals is to inter-operate with other build systems
cleanly. One of ``dds``'s primary outputs is *libman* package indices. These
package indices can be imported into other build systems that support the
*libman* format.

.. note::
    ``dds`` doesn't (yet) have a ready-made central repository of packages that
    can be downloaded. You'll need to populate the local package catalog
    appropriately.

    .. seealso:: Refer to :doc:`catalog` for information about remote packages.

.. _PMM: https://github.com/vector-of-bool/PMM

.. _CMakeCM: https://github.com/vector-of-bool/CMakeCM

.. _lm-cmake: https://raw.githubusercontent.com/vector-of-bool/libman/develop/cmake/libman.cmake


Generating a libman Index
*************************

Importing libman packages into a build system requires that we have a libman
index generated on the filesystem. **This index is not generated globally**: It
is generated on a per-build basis as part of the build setup. The index will
describe in build-system-agnostic terms how to include a set of packages and
libraries as part of a build.

``dds`` has first-class support for generating this index. The ``build-deps``
subcommand of ``dds`` will download and build a set of dependencies, and places
an ``INDEX.lmi`` file that can be used to import the built results.


Declaring Dependencies
======================

``dds build-deps`` accepts a list of dependencies as commnad line arguments,
but it may be useful to specify those requirements in a file.

``dds build-deps`` accepts a JSON5 file describing the dependencies of a
project as well. This file is similar to a very stripped-down version of a
``dds`` :ref:`package manifest <pkgs.pkgs>`, and only includes the ``depends``
key. (The presence of any other key is an error.)

Here is a simple dependencies file that declares a single requirement:

.. code-block:: js
    :caption: ``dependencies.json5``

    {
        depends: {
            'neo-sqlite3': '^0.2.0',
        }
    }


Building Dependencies and the Index
===================================

We can invoke ``dds build-deps`` and give it the path to this file:

.. code-block:: bash

    $ dds build-deps --deps dependencies.json5

When finished, ``dds`` will write the build results into a subdirectory called
``_deps`` and generate a file named ``INDEX.lmi``. This file is ready to be
imported into any build system that can understand libman files (in our case,
CMake).

.. note::
    The output directory and index filepath can be controlled with the
    ``--out`` and ``--lmi-path`` flags, respectively.


Importing into CMake
********************

We've generated a libman index and set of packages, and we want to import
them into CMake. CMake doesn't know how to do this natively, but there exists a
single-file module for CMake that allows CMake to import libraries from libman
indices without any additional work.

The module is not shipped with CMake, but is available online as a single
stand-alone file. The `libman.cmake <lm-cmake_>`_ file can be downloaded and
added to a project directly, or it can be obtained automatically through a
CMake tool like `PMM`_ (recommended).


Enabling *libman* Support in CMake via PMM
==========================================

Refer to the ``README.md`` file in `the PMM repo <PMM_>`_ for information on how
to get PMM into your CMake project. In short, download and place the
``pmm.cmake`` file in your repository, and ``include()`` the file near the top
of your ``CMakeLists.txt``::

    include(pmm.cmake)

Once it has been included, you can call the ``pmm()`` function. To obtain
*libman*, we need to start by enabling `CMakeCM`_::

    pmm(CMakeCM ROLLING)

.. warning::
    It is not recommended to use the ``ROLLING`` mode, but it is the easiest to
    use when getting started. For reproducible and reliable builds, you should
    pin your CMakeCM version using the ``FROM <url>`` argument.

Enabling CMakeCM will make available all of the CMake modules available in `the
CMakeCM repository <CMakeCM_>`_, which includes `libman.cmake <lm-cmake_>`_.

After the call to ``pmm()``, simply ``include()`` the ``libman`` module::

    include(libman)

That's it! The only function from the module that we will care about for now
is the ``import_packages()`` function.


Importing Our Dependencies' Packages
====================================

To import a package from a libman tree, we need only know the *name* of the
package we wish to import. In our example case above, we depend on
``neo-sqlite3``, so we simply call the libman-CMake function
``import_packages()`` with that package name::

    import_packages("neo-sqlite3")

You'll note that we don't request any particular version of the package: All
versioning resolution is handled by ``dds``. You'll also note that we don't
need to specify our transitive dependencies: This is handled by the libman
index that was generated by ``dds``: It will automatically ``import_packages()``
any of the transitive dependencies required.


Using Out Dependencies' Libraries
=================================

Like with ``dds``, CMake wants us to explicitly declare how our build targets
*use* other libraries. When we import a package from a libman index, the
import will generate CMake ``IMPORTED`` targets that can be linked against.

In ``dds`` and in libman, a library is identified by a combination of
*namespace* and *name*, joined together with a slash ``/`` character. This
*qualified name* of a library is decided by the original package author, and
should be documented. In the case of ``neo-sqlite3``, the only target is
``neo/sqlite3``.

When the libman CMake module imports a library, it creates a qualified name
using a double-colon "``::``" instead of a slash. As such, our ``neo/sqlite3``
is imported in CMake as ``neo::sqlite3``. We can link against it as we would
with any other target::

    add_executable(my-application app.cpp)
    target_link_libraries(my-application PRIVATE neo::sqlite3)

Altogether, here is the final CMake file:

.. code-block::
    :caption: ``CMakeLists.txt``
    :linenos:

    cmake_minimum_required(VERSION 3.15)
    project(MyApplication VERSION 1.0.0)

    include(pmm.cmake)
    pmm(CMakeCM ROLLING)

    include(libman)
    import_packages("neo-sqlite3")

    add_executable(my-application app.cpp)
    target_link_libraries(my-application PRIVATE neo::sqlite3)


Additional PMM Support
**********************

The ``pmm()`` function also supports ``dds`` directly, similar to ``CMakeCM``
mode. This will automatically download a prebuilt ``dds`` for the host platform
and invoke ``dds build-deps`` in a single pass as part of CMake's configure
process. This is especially useful for a CI environment where you want to have
a stable ``dds`` version and always have your dependencies obtained
just-in-time.

To start, pass the ``DDS`` argument to ``pmm()`` to use it::

    pmm(DDS)

..note::
    The ``_deps`` directory and ``INDEX.lmi`` file will be placed in the CMake
    build directory, out of the way of the rest of the project.

.. note::
    The version of ``dds`` that PMM downloads depends on the version of PMM
    that is in use.

This alone won't do anything useful, because you'll need to tell it what
dependencies we want to install::

    pmm(DDS DEP_FILES dependencies.json5)

You can also list your dependencies as an inline string in your CMakeLists.txt
instead of a separate file::

    pmm(DDS DEPENDS "neo-sqlite3 ^0.2.2")

Since you'll probably want to be using ``libman.cmake`` at the same time, the
calls for ``CMakeCM`` and ``DDS`` can simply be combined. This is how our new
CMake project might look:

.. code-block::
    :caption: ``CMakeLists.txt``
    :linenos:

    cmake_minimum_required(VERSION 3.15)
    project(MyApplication VERSION 1.0.0)

    include(pmm.cmake)
    pmm(CMakeCM ROLLING
        DDS DEPENDS "neo-sqlite3 ^0.2.2"
        )

    include(libman)
    import_packages("neo-sqlite3")

    add_executable(my-application app.cpp)
    target_link_libraries(my-application PRIVATE neo::sqlite3)

This removes the requirement that we write a separate dependencies file, and we
no longer need to invoke ``dds build-deps`` externally, as it is all handled
by ``pmm``.
