Source Distributions
####################

A *source distribution* (often abbreviated as "sdist") is |bpt|'s primary
format for consuming and distributing packages. A source distribution, in
essence, is a :ref:`package root <pkgs.pkg-root>` that contains only the files
necessary for |bpt| to reproduce the full build of all libraries in the
package. The source distribution retains the directory structure of every
:ref:`source root <pkgs.source-root>` of the original package, and thus retains
the header structure thereof. In this way, the ``#include`` directives to use a
library in a source distribution are identical to if the libraries therein were
directly part of the consuming project.


Generating a Source Distribution
********************************

Generating a source distribution from a project directory is done with the
``pkg`` subcommand::

> bpt pkg create

The above command can be executed within a package root, and the result will be
a gzip'd tar archive that reproduces the package's filesystem structure, but
only maintaining the files that are necessary for |bpt| to reproduce a build
of that package.

The ``--project=<dir>`` flag can be provided to override the directory that
|bpt| will use as the package root. The default is the current working
directory.

The ``--out=<path>`` flag can be provided to override the destination path of
the archive. The path should not name an existing file or directory. By default,
|bpt| will generate a source distribution in the working directory with the
pattern ``<name>@<version>.tar.gz``. If the ``--replace`` flag is provided,
then |bpt| will overwrite the destination if it names an existing file or
directory.


.. _sdist.import:

Importing a Source Distribution
*******************************

Given a source distribution archive, one can import the package into the local
package cache with a single command::

> bpt pkg import some-package@1.2.3.tar.gz

You can also specify an HTTP or HTTPS URL to download a source distribution
archive to import without downloading it separately::

> bpt pkg import https://example.org/path/to/sdist.tar.gz

Alternatively, if a directory correctly models a source distribution, then
that directory can be imported in the same manner::

> bpt pkg import /path/to/some/project

Importing a package will allow projects on the system to use the imported
package as a dependency.

.. note::

    If one tries to import a package root into the cache that already contains a
    package with a matching identifier, |bpt| will issue an error. This
    behavior can be overridden by providing ``--if-exists=replace`` on the
    command-line.
