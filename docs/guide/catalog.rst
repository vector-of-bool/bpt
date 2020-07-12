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

The ``dds catalog import`` supports a ``--json`` flag that specifies a JSON5
file from which catalog entries will be generated.

.. note::
    The ``--json`` flag can be passed more than once to import multiple JSON
    files at once.

The JSON file has the following structure:

.. code-block:: javascript

  {
    // Import version spec.
    version: 1,
    // Packages section
    packages: {
      // Subkeys are package names
      "acme-gadgets": {
        // Keys within the package names are the versions that are
        // available for each package.
        "0.4.2": {
          // `depends` is an array of dependency statements for this
          // particular version of the package. (Optional)
          depends: [
            "acme-widgets^1.4.1"
          ],
          // `description` is an attribute to give a string to describe
          // the package. (Optional)
          description: "A collection of useful gadgets.",
          // Specify the Git remote information
          git: {
            // `url` and `ref` are required.
            url: "http://example.com/git/repo/acme-gadgets.git",
            ref: "v0.4.2-stable",
            // The `auto-lib` is optional, to specify an automatic
            // library name/namespace pair to generate for the
            // root library
            "auto-lib": "Acme/Gadgets",
            // List of filesystem transformations to apply to the repository
            // (optional)
            transform: [
              // ... (see below) ...
            ]
          }
        }
      }
    }
  }


.. _catalog.fs-transform:

Filesystem Transformations
**************************

.. note::
  Filesystem transformations is a transitional feature that is likely to be
  removed in a future release, and replaced with a more robust system when
  ``dds`` has a better way to download packages. Its aim is to allow ``dds``
  projects to use existing libraries that might not meet the layout
  requirements that ``dds`` imposes, but can otherwise be consumed by ``dds``
  with a few tweaks.

A catalog entry can have a set of filesystem transformations attached to its
remote information (e.g. the ``git`` property). When ``dds`` is obtaining a
copy of the code for the package, it will apply the associated transformations
to the filesystem based in the directory of the downloaded/cloned directory. In
this way, ``dds`` can effectively "patch" the filesystem structure of a project
arbitrarily. This allows many software projects to be imported into ``dds``
without needing to patch/fork the original project to support the required
filesystem structure.

.. important::
  While ``dds`` allows you to patch directories downloaded via the catalog, a
  native ``dds`` project must still follow the layout rules.

  The intention of filesystem transformations is to act as a "bridge" that will allow ``dds`` projects to more easily utilize existing libraries.


Available Transformations
=========================

At time of writing, there are five transformations available to catalog entries:

``copy`` and ``move``
  Copies or moves a set of files/directories from one location to another. Allows the following options:

  - ``from`` - The path from which to copy/move. **Required**
  - ``to`` - The destination path for the copy/move. **Required**
  - ``include`` - A list of globbing expressions for files to copy/move. If
    omitted, then all files will be included.
  - ``exclude`` - A list of globbing expressions of files to exclude from the
    copy/move. If omitted, then no files will be excluded. **If both** ``include`` and ``exclude`` are provided, ``include`` will be checked *before* ``exclude``.
  - ``strip-components`` - A positive integer (or zero, the default). When the
    ``from`` path identifies a directory, its contents will be copied/moved
    into the destination and maintain their relative path from the source path as their relative path within the destination. If ``strip-components`` is set to an integer ``N``, then the first ``N`` path components of that relative path will be removed when copying/moving the files in a directory. If a file's relative path has less than ``N`` components, then that file will be excluded from the ``copy/move`` operation.

``remove``
  Delete files and directories from the package source. Has the following options:

  - ``path`` - The path of the file/directory to remove. **Required**
  - ``only-matching`` - A list of globbing expressions for files to remove. If omitted and the path is a directory, then the entire directory will be deleted. If at least one pattern is provided, then directories will be left intact and only non-directory files will be removed. If ``path`` names a non-directory file, then this option has no effect.

``write``
  Write the contents of a string to a file in the package source. Has the following options:

  - ``path`` - The path of the file to write. **Required**
  - ``content`` - A string that will be written to the file. **Required**

  If the file exists and is not a directory, the file will be replaced. If the
  path names an existing directory, an error will be generated.

``edit``
  Modifies the contents of the files in the package.

  - ``path`` - Path to the file to edit. **Required**
  - ``edits`` - An array of edit objects, applied in order, with the following
    keys:

    - ``kind`` - One of ``insert`` or ``delete`` to insert/delete lines,
      respectively. **Required**
    - ``line`` - The line at which to perform the insert/delete. The first line
      of the file is line one, *not* line zero. **Required**
    - ``content`` - For ``insert``, the string content to insert into the file.
      A newline will be appended after the content has been inserted.

Transformations are added as a JSON array to the JSON object that specifies
the remote information for the package. Each element of the array is an
object, with one or more of the keys listed above. If an object features more
than one of the above keys, they are applied in the same order as they have
been listed.


Example: Crypto++
=================

The following catalog entry will build and import `Crypto++`_ for use by a
``dds`` project. This uses the unmodified Crypto++ repository, which ``dds``
doesn't know how to build immediately. With some simple moving of files, we
end up with something ``dds`` can build directly:

.. code-block:: javascript

  "cryptopp": {
    "8.2.0": {
      "git": {
        "url": "https://github.com/weidai11/cryptopp.git",
        "ref": "CRYPTOPP_8_2_0",
        "auto-lib": "cryptopp/cryptopp",
        "transform": [
          {
            // Crypto++ has no source directories at all, and everything lives
            // at the top level. No good for dds.
            //
            // Clients are expected to #include files with a `cryptopp/` prefix,
            // so we need to move the files around so that they match the
            // expected layout:
            "move": {
              // Move from the root of the repo:
              "from": ".",
              // Move files *into* `src/cryptopp`
              "to": "src/cryptopp",
              // Only move the C++ sources and headers:
              "include": [
                "*.c",
                "*.cpp",
                "*.h"
              ]
            }
          }
        ]
      }
    }
  }


Example: libsodium
==================

For example, this catalog entry will build and import `libsodium`_ for use in
a ``dds`` project. This uses the upstream libsodium repository, which does not
meet the layout requirements needed by ``dds``. With a few simple
transformations, we can allow ``dds`` to build and consume libsodium
successfully:

.. code-block:: javascript

  "libsodium": {
    "1.0.18": {
      "git": {
        "url": "https://github.com/jedisct1/libsodium.git",
        "ref": "1.0.18",
        "auto-lib": "sodium/sodium",
        /// Make libsodium look as dds expects of a project.
        "transform": [
          // libsodium has a `src` directory, but it does not look how dds
          // expects it to. The public `#include` root of libsodium lives in
          // a nested subdirectory of `src/`
          {
            "move": {
              // Move the public header root out from that nested subdirectory
              "from": "src/libsodium/include",
              // Put it at `include/` in the top-level
              "to": "include/"
            }
          },
          // libsodium has some files whose contents are generated by a
          // configure script. For demonstration purposes, we don't need most
          // of them, and we can just swipe an existing pre-configured file
          // that is already in the source repository and put it into the
          // public header root.
          {
            "copy": {
              // Generated version header committed to the repository:
              "from": "builds/msvc/version.h",
              // Put it where the configure script would put it:
              "to": "include/sodium/version.h"
            }
          },
          // The subdirectory `src/libsodium/` is no good. It now acts as an
          // unnecessary layer of indirection. We want `src/` to be the root.
          // We can just "lift" the subdirectory:
          {
            // Up we go:
            "move": {
              "from": "src/libsodium",
              "to": "src/"
            },
            // Delete the now-unused subdirectory:
            "remove": {
              "path": "src/libsodium"
            }
          },
          // Lastly, libsodium's source files expect to resolve their header
          // paths differently than they expect of their clients (Bad!!!).
          // Fortunately, we can do a hack to allow the files in `src/` to
          // resolve its headers. The source files use #include as if the
          // header root was `include/sodium/`, rather than `include/`.
          // To work around this, generate a copy of each header file in the
          // source root, but remove the leading path element.
          // Because we have a separate `include/` and `src/` directory, dds
          // will only expose the `include/` directory to clients, and the
          // header copies in `src/` are not externally visible.
          //
          // For example, the `include/sodium/version.h` file is visible to
          // clients as `sodium/version.h`, but libsodium itself tries to
          // include it as `version.h` within its source files. When we copy
          // from `include/`, we grab the relative path to `sodium/version.h`,
          // strip the leading components to get `version.h`, and then join that
          // path with the `to` path to generate the full destination at
          // `src/version.h`
          {
            "copy": {
              "from": "include/",
              "to": "src/",
              "strip-components": 1
            }
          }
        ]
      }
    }
  }

.. _libsodium: https://doc.libsodium.org/
.. _Crypto++: https://cryptopp.com/
