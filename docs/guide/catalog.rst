The Package Catalog
###################

``dds`` stores a catalog of available packages, along with their dependency
statements and information about how a source distribution thereof may be
maintained.


Viewing Catalog Contents
************************

The default catalog database is stored in a user-local location, and the
package IDs available can be listed with ``dds catalog list``. This will only
list the IDs of the packages, but none of the additional metadata about them.


.. _catalog.adding:

Adding Packages to the Catalog
******************************

There are two primary ways to add entries to the package catalog.


Adding Individual Packages
==========================

A single package can be added to the catalog with the ``dds catalog add``
command:

.. code-block:: text

    dds catalog add <package-id>
        [--depends <requirement> [--depends <requirement> [...]]]
        [--git-url <url> --git-ref <ref>]
        [--auto-lib <Namespace>/<Name>]

The ``<package-id>`` positional arguments is the ``name@version`` package ID
that will be added to the catalog. The following options are supported:

``--depends <requirement>``
    This argument, which can be specified multiple times to represent multiple
    dependencies, sets the dependencies of the package within the catalog. If
    the obtained package root contains a ``package.dds``, then the dependencies
    listed here must be identical to those listed in ``package.dds``, or
    dependency resolution may yield unexpected results.

``--git-url <url>``
    Specify a Git URL to clone from to obtain the package. The root of the
    cloned repository must be a package root, but does not necessarily need to
    have the ``package.dds`` and ``library.dds`` files if relying on the
    ``--auto-lib`` parameter.

    ``--git-ref`` **must** be passed with ``--git-url``.

``--git-ref <ref>``
    Specify a Git ref to clone. The remote must support cloning by the ref that
    is specified here. Most usually this should be a Git tag.

    ``dds`` will perform a shallow clone of the package at the specified
    Git reference.

``--auto-lib``
    This option must be provided if the upstream does not already contain the
    ``dds`` files that are necessary to export the library information. This
    can only be specified for packages that contain a single library root at
    the package root.

    The form of the argument is that of ``<Namespapce>/<Name>``, where
    ``Namespace`` and ``Name`` are the usage requirement keys that should be
    generated for the library.


Bulk Imports via JSON
=====================

The ``dds catalog import`` supports a ``--json`` flag that specifies a JSON
file from which catalog entries will be generated.

.. note::
    The ``--json`` flag can be passed more than once to import multiple JSON
    files at once.

The JSON file has the following structure:

.. code-block:: javascript

  {
    // Import version spec.
    "version": 1,
    // Packages section
    "packages": {
      // Subkeys are package names
      "acme-gadgets": {
        // Keys within the package names are the versions that are
        // available for each package.
        "0.4.2": {
          // `depends` is an object of dependencies for this
          // particular version of the package.
          "depends": {
            // A mapping of package names to version ranges
            "acme-widgets": "^1.4.1"
          },
          // Specify the Git remote information
          "git": {
            // `url` and `ref` are required.
            "url": "http://example.com/git/repo/acme-gadgets.git",
            "ref": "v0.4.2-stable",
            // The `auto-lib` is optional, to specify an automatic
            // library name/namespace pair to generate for the
            // root library
            "auto-lib": "Acme/Gadgets"
          }
        }
      }
    }
  }