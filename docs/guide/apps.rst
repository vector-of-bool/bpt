################################
Building Applications with |bpt|
################################

.. default-role:: term

|bpt| will recognize certain compilable `source files <source file>` as
belonging to `applications <application>` (or `tests <test>`), depending on the
`file stem` (the part of the `filename` not including the outer-most
`file extension`). If a compilable source file stem ends with ``.main`` (or
``.test``), that source file is assumed to correspond to an executable to
generate. The filename's second-inner stem before the ``.main`` (or ``.test``)
will be used as the name of the generated executable.

An "`application` source file" is a `source file` whose `file stem` ends with
``.main``. |bpt| will assume such source files to contain a program entry point
function and not include it as part of the `library`'s archive build. Instead,
when |bpt| is generating the library's applications, the source file will be
compiled, and the resulting object will be linked together with the enclosing
library into an executable. For example:

- Given ``foo.main.cpp``

  - The stem is ``foo.main``, which has the extension ``.main``, so we will
    generate an `application`.
  - The stem of ``foo.main`` is ``foo``, so the executable will be named
    ``foo``.

- Given ``cat-meow.main.cpp``

  - The stem is ``cat-meow.main``, which has extension ``.main``, so it is an
    `application`.
  - The stem of ``cat-meow.main`` is ``cat-meow``, so will generate an
    executable named ``cat-meow``.

- Given ``cats.musical.main.cpp``

  - The stem is ``cats.musical.main``, which has extension ``.main``, so this is
    an application.
  - The stem of ``cats.musical.main`` is ``cats.musical``, so we will generate
    an executable named ``cats.musical``.
  - Note that the dot in ``cats.musical`` is not significant, as |bpt| does not
    strip any further extensions.

.. note::

    |bpt| will automatically append the appropriate `file extension` to the
    generated executables based on the host and toolchain.

The building of library applications can be disabled with the
:option:`--no-apps <bpt build --no-apps>` option.

The application source files of `dependencies <dependency>` will be ignored. The
entities defined within application source files will not be included as part of
their enclosing `library`.


Output Path
###########

When an `application` is linked, its output `directory` will be a generated
using a `relative filepath` resolved within the output directory of the build.
The path to the `parent directory <parent filepath>` of the application source file
relative to the `source root` that contains it is used as the subdirectory
within the output directory where the application's executable will be written.

.. csv-table:: Application Output Path

  Application Source `Filepath`, Executable Output Path

  ``src/app.main.cpp``, ``<out>/app``
  ``src/my-app/app.main.cpp``, ``<out>/my-app/app``
  ``src/somedir/acme-widget-maker.main.cpp``, ``<out>/somedir/acme-widget-marker``

.. note::

  The default build output directory is the ``_build/`` directory within which
  |bpt| was invoked. This can be controlled using the
  :option:`--out <bpt build --out>` option.
