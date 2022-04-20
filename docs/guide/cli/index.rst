The |bpt| Command-Line Interface
################################

.. default-role:: term

|bpt| ships as a single stand-alone executable. Brief help on any |bpt|
`command` can be accessed by passing the :option:`--help <bpt --help>` flag.

These documentation pages will explain the `command-line interface` of the |bpt|
executable.

.. seealso:: :doc:`terms`

Base Options
************

The following `command-line arguments` are available for *all* |bpt| commands
and subcommands:

.. program:: bpt

.. option:: --help

    Access the "help" message from any subcommand.

.. option:: --log-level {trace,debug,info,warn,error,critical,silent}

    Set the |bpt| logging level. ``trace`` will emit maximal warning
    information, while ``silent`` will emit no information. All log output is
    written to the ``stderr`` stream.

.. option:: --crs-cache-dir <directory>

    Specify the directory used to store :term:`CRS` package metadata and
    :term:`CRS package` files.

    .. seealso:: The :envvar:`BPT_CRS_CACHE_DIR` environment variable


Base Environment Variables
**************************

The following `environment variables` can be used to control |bpt|'s behavior.
They each correspond to a common |bpt| `command-line arguments`. If |bpt| sees
both an environment variable definition and a given command-line argument, the
command-line argument will take precedence.

.. envvar:: BPT_LOG_LEVEL

    An environment variable that controls :option:`--log-level`.

.. envvar:: BPT_NO_DEFAULT_REPO

    Setting this environment variable to a "truthy" value will imply the
    :option:`--no-default-repo <bpt build --no-default-repo>` flag to any |bpt|
    command that accepts that flag.

.. envvar:: BPT_CRS_CACHE_DIR

    An environment variable that sets the directory where |bpt| will store its
    `CRS` metadata and cached `CRS packages <CRS package>`.

    This environment variable corresponds to the :option:`bpt --crs-cache-dir`
    option.

.. envvar:: BPT_JOBS

    Setting this environment variable to an integer value will specify the
    default for the :option:`--jobs <bpt-build --jobs>` option. This can be used
    to control the concurrency of |bpt| subprocesses that you may not have
    access to directly change the `command` invocations.

.. _cli.subcommands:

|bpt| Subcommands
*****************

|bpt| defines the following top-level subcommands:

- :doc:`build`
- :doc:`compile-file`
- :doc:`build-deps`
- :doc:`pkg`
- :doc:`repo`
- :doc:`install-yourself`
- :doc:`new`

.. toctree::
    :hidden:

    build
    compile-file
    build-deps
    pkg
    repo
    install-yourself
    new
    terms
