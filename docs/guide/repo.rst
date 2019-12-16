The Local Package Repository
############################

``dds`` maintains a local repository of packages that it has obtained at the
request of a user. The packages themselves are stored as
:doc:`source distributions <source-dists>` (``dds`` does not store the binaries
that it builds within the repository).


Reading Repository Contents
***************************

Most times, ``dds`` will manage the repository content silently, but it may be
useful to see what ``dds`` is currently storing away.

The content of the repostiory can be seen with the ``repo`` subcommand::

> dds repo ls

This will print the names of packages that ``dds`` has downloaded, as well as
the versions of each.


Obtaining Packages
******************

.. seealso:: See also: :doc:`catalog`

When ``dds`` builds a package, it will also build the dependency libraries of
that package. In order for the dependency build to succeed, it must have a
local copy of the source distribution of that dependency.

When ``dds`` performs dependency resolution, it will consider both existing
packages in the local repository, as well as packages that are available from
the :doc:`package catalog <catalog>`. If the dependency solution requires any
packages that are not in the local repository, it will use the information in
the catalog to obtain a source distribution for each missing package. These
source distributions will automatically be added to the local repository, and
later dependency resolutions will not need to download that package again.


Manually Downloading a Dependency
=================================

It may be useful to obtain a copy of the source distribution of a package
contained in the catalog. The ``catalog get`` command can be used to do this::

> dds catalog get <name>@<version>

This will obtain the source distribution of the package matching the named
identifier and place that distribution in current working directory, using the
package ID as the name of the source distribution directory::

    $ dds catalog get spdlog@1.4.2
    [ ... ]

    $ ls .
    .
    ..
    spdlog@1.4.2

    $ ls ./spdlog@1.4.2/
    include/
    src/
    library.dds
    package.dds


.. _repo.export-local:

Exporting a Project into the Repository
***************************************

``dds`` can only use packages that are available in the local repository. For
packages that have a listing in the catalog, this is not a problem. But if one
is developing a local package and wants to allow it to be used in another local
package, that can be done by exporting a source distribution from the package
root::

> dds sdist export

This command will create a source distribution and place it in the local
repository. The package is now available to other projects on the local system.

.. note::
    This doesn't export in "editable" mode: A snapshot of the package root
    will be taken and exported to the local repository.

If one tries to export a package root into a repository that already contains
a package with a matching identifier, ``dds`` will issue an error. If the
``--replace`` flag is specified with ``sdist export``, then ``dds`` will
forcibly replace the package in the local repository with a new copy.
