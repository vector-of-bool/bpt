########
Packages
########

.. |version| replace:: :hoverxreftooltip:`version <semver>`

.. |name| replace:: :doc:`name </crs/names>`

.. default-role:: term

A CRS package is the unit of distribution in CRS. A package has a |name| and a
|version| and contains one or more `libraries <CRS library>`.


Encoded Attributes
##################

A CRS package encodes the following high-level attributes:

.. glossary::

  package name

    The identity of the software that is being distributed in the package.
    (Must be a valid |name|.)

  package version

    A version string for the package itself. At the time of writing, this is
    required to be a :hoverxref:`Semantic Version String <semver>`.
    Additional versioning formats may be considered in the future.

  package meta-version

    An integer constant that encodes the revision of the package itself,
    regardless of the version of the source code inside.

  package libraries

    A list of libraries contained within the package. A single package may
    contain any number of libraries. Libraries have further attributes, such as
    :term:`library internal usages` and :term:`library external dependencies`

    .. seealso:: :doc:`libraries`.


Dependencies
############

Unlike most packaging formats, dependencies in `CRS` are not associated with
packages at the top-level. Instead, dependencies are associated with individual
libraries within a package.

.. seealso::

  - :ref:`Libraries\: External Dependencies <crs.library.dependencies>`
  - :doc:`The documentation on dependencies and their attributes <dependencies>`
