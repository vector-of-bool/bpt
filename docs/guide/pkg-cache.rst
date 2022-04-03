The Local Package Cache
#######################

|bpt| maintains a local cache of packages that it has obtained at the request
of a user. The packages themselves are stored as
:doc:`source distributions <source-dists>` (|bpt| does not store the binaries
that it builds within this package cache).


Reading Repository Contents
***************************

Most times, |bpt| will manage the cache content silently, but it may be useful
to see what |bpt| is currently storing away.

The content of the cache can be seen with the ``pkg ls`` subcommand::

> bpt pkg ls

This will print the names of packages that |bpt| has downloaded, as well as
the versions of each.


Obtaining Packages
******************

.. seealso:: See also: :doc:`remote-pkgs`

When |bpt| builds a package, it will also build the dependency libraries of
that package. In order for the dependency build to succeed, it must have a
local copy of the source distribution of that dependency.

When |bpt| performs dependency resolution, it will consider both locally
cached packages, as well as packages that are available from any
:doc:`remote packages <remote-pkgs>`. If the dependency solution requires any
packages that are not in the local cache, it will use the information in its
catalog database to obtain a source distribution for each missing package. These
source distributions will automatically be added to the local cache, and later
dependency resolutions will not need to download that package again.

This all happens automatically when a project is built: There is **no**
"``bpt install``" subcommand.


Manually Downloading a Dependency
=================================

It may be useful to obtain a copy of the source distribution of a package
from a remote. The ``pkg get`` command can be used to do this::

> bpt pkg get <name>@<version>

This will obtain the source distribution of the package matching the given
package ID and place that distribution in current working directory, using the
package ID as the name of the source distribution directory::

    $ bpt pkg get spdlog@1.4.2
    [ ... ]

    $ ls .
    .
    ..
    spdlog@1.4.2

    $ ls ./spdlog@1.4.2/
    include/
    src/
    library.json5
    package.json5


.. _repo.import-local:

Exporting a Project into the Repository
***************************************

|bpt| can only use packages that are available in the local cache. For
packages that have a listing in the catalog, this is not a problem. But if one
is developing a local package and wants to allow it to be used in another local
package, that can be done by importing that project into the package cache as a
regular package, as detailed in :ref:`sdist.import`::

> bpt pkg import /path/to/project

This command will create a source distribution and place it in the local cache.
The package is now available to other projects on the local system.

.. note::
    This doesn't import in "editable" mode: A snapshot of the package root
    will be taken and imported to the local cache.
