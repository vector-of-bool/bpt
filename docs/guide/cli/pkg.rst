``bpt pkg``
###########

``bpt pkg`` is a family of subcommands that can be used to create and inspect
packages.

.. _cli.pkg-search:

``bpt pkg search``
******************

``bpt pkg search`` will search for packages available in one or more
repositories.

.. program:: bpt pkg search

.. option:: <name-or-pattern>

    A name or glob-style pattern to search for. Matching packages will be listed
    in the command output. Searching is case-insensitive. Only the name of the
    package will be matched (not the version). If omitted, all packages will be
    listed.

.. include:: ./repo-common-args.rst


``bpt pkg create``
******************

``bpt pkg create`` will generate a :term:`CRS package` from a |bpt| project.

.. program:: bpt pkg create

.. include:: ./opt-project.rst

.. option:: --out <path>, -o <path>

    Specify the :term:`filepath` at which the :term:`CRS package` archive file
    will be written. The default is a filename based on the name and version of
    the input package.

.. option:: --if-exists {replace,ignore,fail}

    Specify the behavior of |bpt| in case the output package file already
    exists. The default value is ``fail``.


``bpt pkg prefetch``
********************

``bpt pkg prefetch`` can be used to fetch packages and package metadata from
remotes so that it will be immediately available in subsequent operations.

.. note::

    This only pulls packages into the :term:`CRS` package cache (specified by
    :option:`bpt --crs-cache-dir`). It can be used to pre-seed the package cache
    before subsequent operations need to pull those packages.

.. program:: bpt pkg prefetch

.. option:: <pkg-id> ...

    Any number of package IDs to fetch from repositories. Package IDs are of the
    form ``{name}@{version}``.

.. include:: ./repo-common-args.rst


``bpt pkg solve``
*****************

``bpt pkg solve`` will attempt to generate a dependency solution from the given
dependency strings specified on the command line.

The requirements will be printed as the output to the |bpt| command.

.. program:: bpt pkg solve

.. option:: <requirement> ...

    One or more dependency statements for which |bpt| will to try and generate a
    solution.

.. include:: ./repo-common-args.rst
