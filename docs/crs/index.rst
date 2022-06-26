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

.. note::

    This documentation, and `CRS` itself, are both still works-in-progress. It
    may be incomplete and feature errors in speling and grammars.


Topics
######

.. toctree::

    packages
    libraries
    dependencies
    versions
    names
    json


Base Concepts
#############

Briefly, `CRS` concerns itself with the following high-level concepts:

.. glossary::

  CRS

    An acronym for :strong:`C`\ ompile-\ :strong:`R`\ eady :strong:`S`\ ource.
    CRS is a set of file formats and `JSON` schemas that describe a way to
    distribute software in source-code format, ready to be given to any
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




Dependencies
************



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
