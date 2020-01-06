Source Distributions
####################

A *source distribution* is ``dds``'s primary format for consuming and
distributing packages. A source distribution, in essence, is a
:ref:`package root <pkgs.pkg-root>` directory that contains only the files
necessary for ``dds`` to reproduce the full build of all libraries in the
package. The source distribution retains the directory structure of every
:ref:`source root <pkgs.source-root>` of the original package, and thus retains
the header structure thereof. In this way, the ``#include`` directives to use
a library in a source distribution are identical to if the libraries therein
were directly part of the consuming project.


Generating a Source Distribution
********************************

Generating a source distribution from a project directory is done with the
``sdist`` subcommand::

> dds sdist create

The above command can be executed within any package root, and the result will
be a new directory that reproduces the package's filesystem structure, but
only maintaining the files that are necessary for ``dds`` to reproduce the
build of that package.

The ``--project=<dir>`` flag can be provided to override the directory that
``dds`` will use as the package root. The default is the working directory of
the project.

The ``--out=<path>`` flag can be provided to override the destination path of
the resulting source distribution. The path should not name an existing file or
directory. By default, ``dds`` will generate a source distribution in the
working directory with the name ``project.dsd/`` (The output is itself a
directory, not an archive). If the ``--replace`` flag is provided, then ``dds``
will overwrite the destination if it names an existing file or directory.


Exporting a Package to the Local Repository
*******************************************

.. seealso:: :ref:`repo.export-local`
