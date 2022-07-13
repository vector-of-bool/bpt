``bpt repo``
############

.. default-role:: term

``bpt repo`` is used to create and manage repository directories.

.. note::

    This command is used to manage the repositories upstream, and is not
    relevant to managing the repositories used on the client-side.

    During builds, |bpt| uses options such as :option:`bpt build --use-repo` to
    control repositories used for a build.


.. _cli.repo-init:

``bpt repo init``
*****************

``bpt repo init <repo-dir>`` will initialize a new `CRS` repository in the given
directory

.. program:: bpt repo init

.. option:: <repo-dir>

  The `filepath` to a `directory` in which to initialize a new repository.

.. option:: --name <name>

  :required: Specify the name of the new repository. This can be any free-form
      string. It is recommended to use the domain name (and possible URL path)
      at which the repository is intended to be accessed. This string is not
      usually seen in the |bpt| user interface.

  .. important::

    It is essential that the name of the repository be *globally* unique, as it
    is used as the cache key in |bpt| when storing package metadata.

.. option:: --if-exists {replace,ignore,fail}

  The action to perform if there is already a repository present in the given
  :option:`\<repo-dir\>`.

  ``fail`` (The default)
    Report an error and exit non-zero.

  ``ignore``
    Do no action. Reports a notice and exits zero.

  ``replace``
    Delete the existing repository database and create a new one in its place.


``bpt repo import``
*******************

``bpt repo import <repo-dir>`` will import one or more `CRS package` archive
files into a repository at the given directory.

.. program:: bpt repo import

.. option:: <repo-dir>

  .. |repo-dir-arg| replace::

    The repository directory to managed. Must have been initialized using
    :ref:`cli.repo-init`.

  |repo-dir-arg|

.. option:: <crs-path> ...

  The `filepath` to one or more `CRS packages <CRS package>` or |bpt| projects
  to import.

  If the given path points to a |bpt| project, a CRS package archive will be
  generated on-the-fly for the project.

  Any number of package paths may be provided.

.. option:: --if-exists {replace,ignore,fail}

  The action to perform if any of the given :option:`\<crs-path\>` paths
  identify packages which are already present in the repository at
  :option:`\<repo-dir\>`.

  ``fail`` (The default)
    Report an error and exit non-zero.

  ``ignore``
    Skip the already-present package.

  ``replace``
    Delete the existing package entry and create a new one in its place.


``bpt repo remove``
*******************

``bpt repo remove <repo-dir>`` will remove packages from a repository.

.. program:: bpt repo remove

.. option:: <repo-dir>

  |repo-dir-arg|

.. option:: <pkg-id> ...

  The ``{name}@{version}~{revision}`` identifiers of packages to remove. Can be
  provided multiple times.


``bpt pkg ls``
**************

``bpt pkg ls <repo-dir>`` will print a list of the packages in a repository.
Each package will be of the form ``{name}@{version}~{revision}``, one package ID
per line.

.. program:: bpt repo ls

.. option:: <repo-dir>

  |repo-dir-arg|


``bpt repo validate``
*********************

``bpt repo validate <repo-dir>`` validates that every package individually can
be installed using only other packages in the same repository.

For every package in the repository, |bpt| will attempt to form a valid
dependency solution thereof, using only packages in that same repo as dependency
candidates

.. program:: bpt repo validate

.. option:: <repo-dir>

  |repo-dir-arg|
