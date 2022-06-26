##################
Package Versioning
##################

CRS Package versioning is based on `Semantic Versioning`_

.. _semver:

Semantic Versioning
###################

:title-reference:`Semantic Versioning` is a standard of software versioning that
defines a version string format and assigns semantics to each component of that
string.

Examples of valid Semantic Version numbers:

- ``1.2.3``
- ``2.0.0``
- ``2.0.5``
- ``3.0.0-beta.1``
- ``4.5.0-dev+2022.3.7``


.. rubric:: Basic Syntax

A basic Semantic Version string consists of three numeric values separated with
a dot "``.``"::

    X.Y.Z

These numeric components are referred to as *major*, *minor*, and *patch*,
respectively::

    <major>.<minor>.<patch>


.. rubric:: Prereleases

A Semantic Version string can also contain a *prerelease* suffix. A prerelease
suffix is attached to a base version number with a hyphen "``-``". The suffix is
an arbitrary sequence of alphanumeric identifiers separated by a dot "``.``"::

    <major>.<minor>.<minor>-<prerelease>


.. seealso::

    For more information on Semantic Versioning, refer to https://semver.org/
