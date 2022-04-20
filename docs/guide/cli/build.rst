``bpt build``
#############

``bpt build`` can be used to compile, link, and test a project or package. It
accepts the following options:

.. program:: bpt build

.. include:: ./opt-toolchain.rst
.. include:: ./opt-project.rst
.. include:: ./opt-out.rst

.. option:: --no-tests

  Disabling compiling+linking of the project's tests.


.. option:: --no-apps

  Disable compiling+linking of the project's applications.


.. option:: --no-warn

  Disable compile/link warnings when building the project. Warning options are
  specified as part of the :doc:`toolchain </guide/toolchains>` in use.

.. include:: ./opt-tweaks-dir.rst
.. include:: ./opt-jobs.rst
.. include:: ./repo-common-args.rst
