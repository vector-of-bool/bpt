########################
Writing Tests with |bpt|
########################

.. default-role:: term

|bpt| will recognize certain compilable `source files <source file>` as
belonging to `tests <test>` (or `applications <application>`), depending on the
`file stem` (the part of the `filename` not including the outer-most
`file extension`). If a compilable source file stem ends with ``.test`` (or
``.main``), that source file is assumed to correspond to an executable to
generate. The filename's second-inner stem before the ``.test`` (or ``.main``)
will be used as the name of the generated executable.

A "`test` source file" is a `source file` whose `file stem` ends with ``.test``.
When building a `project`, |bpt| will generate an executable program for each
such source file, compiling and linking it with the enclosing library and the
library's `test dependencies <test dependency>`. The name of the test will be
generated from the next-inner stem of the filename:

- Given ``bar.test.cpp``

  - The stem is ``bar.test``, whose extension is ``.test``, so we will generate
    a test.
  - The stem of ``bar.test`` is ``bar``, so will generate an executable named
    ``bar``.

- Given ``cats.musical.test.cpp``

  - The stem is ``cats.musical.test``, which has extension ``.test``, so this is
    a text executable.
  - The stem of ``cats.musical.test`` is ``cats.musical``, so we will generate
    an executable named ``cats.musical``.
  - Note that the dot in ``cats.musical`` is not significant, as |bpt| does
    strip any further extensions.

.. note::

    |bpt| will automatically append the appropriate `file extension` to the
    generated executables based on the host and toolchain.

The compiling and linking of a project's tests can be disabled with the
:option:`--no-tests <bpt-build --no-tests>` option. This will also omit the
project's `test dependencies <test dependency>` from the dependency solution.

The test source files of `dependencies <dependency>` will be ignored.


Output Path
###########

When a `test` is linked, its output `directory` will be a generated using a
`relative filepath` resolved within the ``test/`` subdirectory of the output
directory of the build. The path to the `parent directory <parent filepath>` of
the test source file relative to the `source root` that contains it is used as
the subdirectory within the ``test/`` directory where the test's executable will
be written.

.. csv-table:: Application Output Path

  Test Source `Filepath`, Test Executable Output Path

  ``src/checks.test.cpp``, ``<out>/test/checks``
  ``src/my-lib/testing.test.cpp``, ``<out>/test/my-lib/testing``
  ``src/somedir/widgets.test.cpp``, ``<out>/test/somedir/widgets``

.. note::

  The default build output directory is the ``_build/`` directory within which
  |bpt| was invoked. This can be controlled using the
  :option:`--out <bpt build --out>` option.
