CRS Package Storage
###################

.. |name| replace:: :doc:`name </crs/names>`

A CRS package can be represented in any system that allows heirarchical data
storage with named files and directories. This may be a regular directory on
disk, a Zip archive, a Tar archive, a database table, or even something as
simple as a single large `JSON` document.


.. _pkg.json:

``pkg.json``
************

At the root of the package must be a valid `JSON` document named ``pkg.json``.
This file encodes all of the metadata in the package.

At any point in the `JSON` object, if any `JSON` object key string begins with
the substring "``_comment``", then content of that object entry is to be ignored
for validating the `JSON` data.

The basic interface of ``pkg.json`` looks as below:

.. code-block:: javascript

    interface CRSPackage {
        $schema?: string,
        "schema-version": 0,
        name: NameString,
        version: VersionString,
        "pkg-version": integer,
        libraries: CRSLibrary[],
        meta?: {
            [key: string]: unknown
        },
        extra?: null | {
            [key: string]: unknown
        },
    }

    interface CRSLibrary {
        name: NameString,
        path: string,
        using: NameString[],
        dependencies: CRSDependency[],
        "test-dependencies": CRSDependency[],
    }

    interface CRSDependency {
        name: NameString,
        using: NameString,
        versions: VersionRange[],
    }

    interface VersionRange {
        low: VersionString,
        high: VersionString,
    }

Where "``NameString``" is a string that is a valid :doc:`CRS name <names>` and
"``VersionString``" is a string that is a valid
:hoverxref:`Semantic Version string <semver>`.


.. default-domain:: js

.. _crs.package:

Schema
******

.. default-domain:: schematic

.. mapping:: CRSPackage_v0

  A JSON object type. Any properties that allow :ts:`undefined` are optional,
  and all others are required.

  .. property:: schema-version
    :required:

    :type: Literal :ts:`0` (zero)

    The version of the `CRS` schema described by the object. For this type at
    this version, the value is literal zero :ts:`0`.

    Package processors should validate the content of this property before
    checking any other content in the document.

  .. property:: name
    :required:

    :type: string

    The name of the package. Must be a valid :doc:`CRS name <names>`.

  .. property:: version
    :required:

    :type: string

    The version of the software that is contained in the enclosing package. Must
    be a valid :ref:`semver` version string.

    .. note:: Do not confuse with the :schematic:prop:`pkg-version` property.

  .. property:: pkg-version
    :required:

    :type: integer

    Used to record revisions of the package itself. This is used when the
    content of the package's metadata may require changing, but the content of
    the packaged software is equivalent to the content of prior revisions. Must
    be greater or equal to :ts:`1` (one).

  .. property:: libraries
    :required:

    :type: CRSLibrary_v0[]

    Libraries of the package. Must be a non-empty array of
    :schematic:mapping:`CRSLibrary_v0` objects.

  .. property:: meta

    :type: undefined | null | unknown | null

    Used to attach metadata to the package that isn't required for dependency
    resolution nor build systems. Package processors should not mandate any
    content in the nested ``meta`` object.

  .. property:: extra
    :optional:

    :type: undefined | null

    An optional JSON object or ``null`` that encodes additional tool-specific
    attributes for the package.

  .. property:: $schema
    :optional:

    :type: :ts:`string | undefined`

    A property using by JSON-schema validators. Do not parse nor validate this.

  .. property:: _comment
    :optional:

    :type: :ts:`string | undefined`

    Ignore these


.. mapping:: CRSLibrary_v0

  An object type that contains properties of CRS libraries


.. default-domain:: js

.. _crs.libraries:

Libraries
*********

In |pkg.json|, the ``libraries`` property must be an array of CRS library JSON
objects.

Each object in that array defines a library for the package. Each object must
contain the following properties:


``name``
========

The name of the library. :doc:`Must be a valid name. <names>`

No two libraries within a single package may share a name.


``path``
========

The path to the library root. This must be a valid UTF-8 POSIX-style relative
filepath using forward-slash "``/``" (solidus) directory separators. The path
may not contain any backslash "``\``" characters.

A library path must be normalized and validated according to the following
rules:

1. If the first character in the path is a forward-slash, the path is invalid
   (absolute paths are not allowed).
2. For every sequence of two or more forward-slashes in the path string, replace
   the sequence with a single forward-slash.
3. Remove a final forward-slash in the path string, if present.
4. Split the path string on forward-slashes into a list of *component* strings.
5. Remove every component from the array that is a single ASCII dot "``.``".
6. For each *component* in the path string that is a dot-dot "``..``":

   1. If the dot-dot component is the first component in the component list, the
      path is invalid (The path attempts to reach outside of the CRS package).
   2. Delete the dot-dot component and the component preceeding it in the array.

7. All remaining path components must also be valid :doc:`CRS names <names>`, except
   that they may begin with an ASCII digit. Otherwise the path is invalid.
8. Join the component array with forward-slashes in-between each component. This
   is the new path.
9. If the path is an empty string, the path is "``.``".

After normalizing the library's path, no two libraries may have the same path.
Some platforms may place additional restrictions on library paths.


``using``
=========

A list of :doc:`names` of other libraries within the same package. Each string must
correspond to the ``name`` property on some other library in the package. A
library cannot "use" itself. The chain of ``using`` should form a acyclic graph.
If a ``using`` string names a non-existent library, or if there is a cycle in
the ``using`` graph, the entire package is invalid.


``dependencies``
================

An array of `CRS dependency` objects.

This array specifies the dependencies of the library within the package.


``test-dependencies``
=====================

An array of `CRS dependency` objects.

This array specifies the test-only dependencies of the library within the
package.



Dependencies
############

Unlike most packaging formats, dependencies in `CRS` are not associated with
packages at the top-level. Instead, dependencies are associated with individual
libraries within a package.

.. seealso::

  - :ref:`Libraries\: External Dependencies <crs.library.dependencies>`
  - :doc:`The documentation on dependencies and their attributes <dependencies>`

A CRS dependency identifies a package by name, one or more version ranges, and
some set of used-libraries. A dependency is associated with a
`library <CRS library>` within a package, and not with the package as a whole.

A CRS dependency is specified as a JSON object with the following required
properties:

.. default-domain:: schematic

.. mapping:: CRSDependency_v0

  .. property:: name
    :required:

    :type: :ts:`string`

    The name of a package which is being depended-upon.
    :doc:`Must be a valid name. <names>`

  .. property:: using
    :required:

    :type: :ts:`string[]`

    An array of |name| strings identifying libraries within the depended-upon
    package to be used by the library that contains this dependency.

  .. property:: versions
    :required:

    An array of one or more *version range* objects. Each JSON object must contain
    the following properties:

    ``low``
      The minimum version of the range. Must be a valid
      :hoverxref:`Semantic Version string <semver>`.

    ``high``
      The maximum version of the range, exclusive. Must be a valid
      :hoverxref:`Semantic Version string <semver>`.