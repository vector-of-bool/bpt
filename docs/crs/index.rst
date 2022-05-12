########################
The CRS Packaging Format
########################

.. highlight:: javascript

.. |version| replace:: :hoverxreftooltip:`version <semver>`

.. |name| replace:: :doc:`name </crs/names>`

.. default-role:: term

`CRS`, an initialism for ":strong:`C`\ ompile-:strong:`R`\ eady :strong:`S`\
ource", is a package format with an emphasis on simplicity and embeddability. A
`CRS package` is so-called "compile-ready" because it contains package source
files that do not require preparation, configuration, generation, or tweaking
before being given to a compiler.

.. note::

    The `CRS` format is intended to be portable and not directly tied to |bpt|
    itself.

Topics
######

.. toctree::

    packages
    libraries
    dependencies
    versions
    names

Base Concepts
#############

Briefly, `CRS` concerns itself with the following high-level concepts:

.. glossary::

  CRS

    An acronym for :strong:`C`\ ompile-\ :strong:`R`\ eady :strong:`S`\ ource.
    CRS is a set of file formats and `JSON` schemas that describe a way to
    destribute software in source-code format, ready to be given to any
    compatible compiler without further configuration or code generation.

    .. seealso:: :doc:`index`

  CRS package

    A CRS package is the unit of distribution in `CRS`. A package has a |name|
    and a |version| and contains one or more `CRS libraries <CRS library>`.

    .. seealso:: :doc:`The documentation page about packages <packages>`

  CRS library

    A CRS library is the smallest consumable unit in a `CRS package`. A package
    can have one or more libraries, but a library can only belong to one
    package. Each library within a single package must have a unique |name|. A
    library can have some number of `CRS dependencies <CRS dependency>`, and can
    "`use <library usage>`" other libraries within the same package.

    .. seealso:: :doc:`The documentation page about libraries <libraries>`

  CRS dependency

    A CRS dependency is associated with a `CRS library` within a `CRS package`.
    A dependency names a package, one or more libraries within that package, and
    one or more compatible version ranges.

    .. seealso::

      :doc:`The documentation page about dependencies <dependencies>`


Storage Format
##############

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

    interface PKGJSON {
        "schema-version": 1,
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

The root of this JSON data must be a JSON object with the following *required*
keys:

``schema-version``
==================

An integer encoding the version of the CRS metadata itself. For this
documentation, this number must be ``1``.

If the ``schema-version`` is greater than the version supported by the system,
the package must be rejected as invalid.

``name``
========

A string. Specifies the name of a package. :doc:`Must be a valid name. <names>`

``version``
===========

A string. Specifies the version of the package. Must be a valid
:hoverxref:`Semantic Version string <semver>`.

``pkg-version``
===============

An integer. Specifies the version of the package itself (not related to the
version of the software that is packaged).

If the ``pkg-version`` is not a positive non-zero whole integer, the package is
invalid.

``libraries``
=============

An array of library definitions for the package. Refer to: :ref:`crs.libraries`.
If any of the libraries in the array are invalid, the entire package is invalid.

``meta``
========

An optional JSON object containing metadata attributes for the package intended
for human consumption.

``extra``
=========

An optional JSON object or ``null`` that encodes additional tool-specific
attributes for the package.

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
************

A CRS dependency identifies a package by name, one or more version ranges, and
some set of used-libraries. A dependency is associated with a
`library <CRS library>` within a package, and not with the package as a whole.

A CRS dependency is specified as a JSON object with the following required
properties:


``name``
========

The name of a package which is being depended-upon.
:doc:`Must be a valid name. <names>`


``using``
=========

An array of |name| strings identifying libraries within the depended-upon
package to be used by the library that contains this dependency.


``versions``
============

An array of one or more *version range* objects. Each JSON object must contain
the following properties:

``low``
    The minimum version of the range. Must be a valid
    :hoverxref:`Semantic Version string <semver>`.

``high``
    The maximum version of the range, exclusive. Must be a valid
    :hoverxref:`Semantic Version string <semver>`.


Range Semantics
***************

Versions should be compared using the version ordering specified by
:ref:`SemVer`.

For the purpose of version comparison, a synthesized version number "synth-high"
refers to the version specified by ``high`` with an additional
lowest-possible-precedence prerelease identifier attached. It is impossible to
spell this synthesized semantic version number as a valid string, but it is
required to obtain half-open range semantics on version ranges. For purposes of
examples here, this lowest-possible-precedence prerelease identifier will be
represented as a percent-symbol "``%``".

If given a version ``V`` and a ``low`` and ``high`` range, ``V`` is considered
to be within the version range if it is greater-than or equal-to ``low``, and
less-than "synth-high".


Range Comparison Example
========================

Suppose a version range is provided::

    {
        "low": "2.8.0",
        "high": "3.0.0"
    }

and the following versions are available:

- ``1.2.6``
- ``2.5.6``
- ``2.7.5``
- ``2.8.0-beta.1``
- ``3.0.0-alpha``
- ``3.1.0``

It is easy to see that ``1.2.6``, ``2.5.6``, and ``2.7.5`` are too old, as they
compare less-than the ``low`` version. The ``3.1.0`` version is greater-than the
``high`` version, so it is also not within the range.

The ``2.8.0-beta.1`` version is less-than our low ``2.8.0`` requirement, since
prerelease versions are strictly less-than a non-prerelease with the same
version number components.

The ``3.0.0-alpha`` version *is* within the half-open version range
``[2.8.0, 3.0.0)``, because ``3.0.0-alpha`` is *less-than* ``3.0.0``. However,
selecting this version to satisfy the range would be very surprising and
unlikely to be what the user wanted. For this reason, the synthesized prerelease
"high" version ``3.0.0-%`` is used for the range comparison. The imaginary
"``%``" prerelease identifier is strictly less than all other possible
prerelease identifiers. For this reason, ``3.0.0-alpha`` is *not* within the
half-open version range ``[2.8.0, 3.0.0-%)``, and therefore does not satisfy the
dependency version range JSON object provided.
