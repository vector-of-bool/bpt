Error: One or more tests failed
###############################

This error message is printed when a project's tests encounter a failure
condition. The exact behavior of tests is determined by a project's
``test_driver``.

If you see this error, it is most likely that you have an issue in the tests of
your project.

When a project is built with the ``build`` command, it is the default behavior
for ``dds`` to compile, link, and execute all tests defined for the project.
Test execution can be suppressed using the ``--no-tests`` command line option
with the ``build`` subcommand.

.. seealso::
    Refer to the page on :ref:`pkgs.apps-tests`.