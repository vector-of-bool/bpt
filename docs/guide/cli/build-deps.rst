``bpt build-deps``
##################

The ``bpt build-deps`` command is used to compile a set of dependencies for use
in another project.

.. program:: bpt build-deps

.. option:: <dependency> ...

    One or more dependency strings. These match the ``dependencies`` strings
    used in ``bpt.yaml``.

.. include:: ./opt-toolchain.rst
.. include:: ./opt-tweaks-dir.rst
.. include:: ./opt-out.rst

.. option:: --built-json <json-path>

    Specify the destination :term:`filepath` of the generated ``_built.json``
    file (Refer: :doc:`/guide/build-deps`)

.. option:: --cmake <file-path>

    Generate a CMake script that will create imported targets for the
    dependencies that |bpt| builds.

.. include:: ./opt-jobs.rst
.. include:: ./repo-common-args.rst
