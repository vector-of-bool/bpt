``bpt compile-file``
####################

``bpt compile-file`` can be used to compile individual source files in a project
or dependency. It accepts the following options:

.. program:: bpt compile-file

.. option:: <filepath> ...

    Specify any number of source files that are compiled as part of the project.
    |bpt| will compile these files. If any named files are not a part of the
    project, |bpt| will fail with an error.

.. include:: ./opt-toolchain.rst
.. include:: ./opt-project.rst

.. option:: --no-warn

    Disable compiler warnings when compiling the given source files.

.. include:: ./opt-out.rst
.. include:: ./opt-tweaks-dir.rst
.. include:: ./repo-common-args.rst

