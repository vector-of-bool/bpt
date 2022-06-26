###################
Projects & Packages
###################

.. default-role:: term
.. highlight:: yaml

For |bpt| projects and packages, there are a few important terms to understand:

.. glossary::

  project

    A |bpt| *project* is a `directory` containing a |bpt.yaml| file and
    defines one or more `libraries <library>`.

    Like a `package`, a project has a `name` and a :ref:`version <semver>`. For
    many purposes, |bpt| will treat a project as a package with special
    properties.

    |bpt| can capture a project directory as a package for distribution and use
    in other tools by using the :ref:`cli.pkg-create` subcommand.

    .. seealso:: :ref:`guide.projects`

  package

    A *package* is a `named <name>` and :ref:`versioned <semver>` collection of
    `libraries <library>` distributed as a unit, available to be "used" to build
    additional libraries or `applications <application>`.

    A package contains a |pkg.json| file (whereas a `project` would contain a
    |bpt.yaml| file).

    When |bpt| is building a `dependency` solution from some set of dependency
    statements, the `name` of the packages are used to create uniqueness: For
    every package in a dependency solution, each `name` may only resolve to only
    a single version.

    .. seealso:: :term:`CRS package`

  dependency

    A *dependency* specifies a requirement on `libraries <library>` in an
    external `package` in order to build and use the library that has the
    dependency.

    A dependency specifies the `name` of an external package, a range of
    compatible :ref:`versions <semver>`, and a list of one or more libraries
    from the depended-on package that are required.

    In |bpt| (and `CRS`), dependencies are attached to individual libraries, and
    not to the package that contains that library.

    .. seealso:: :term:`CRS dependency`


.. _guide.projects:

Understanding Projects
######################

When using |bpt|, one is most often working within the scope of a project. A
`directory` is a |bpt| *project root* if it contains a ``bpt.yaml``:

.. glossary::

  ``bpt.yaml``

    The ``bpt.yaml`` file is placed in the root `directory` of a `project`. It
    defines the `package` attributes of the project, including the `name`,
    :ref:`version <semver>`, `libraries <library>`, and the
    `dependencies <dependency>` of those libraries. The complete file schema can
    be found here: :schematic:mapping:`Project`.

    .. rubric:: Example

    .. code-block:: yaml
      :linenos:

      # Required: The name of the project
      name: my-example-package
      # Required: The version of the project
      version: 2.5.1-dev

    Within a |bpt.yaml| file, only the :yaml:`name` and :yaml:`version` keys are
    required.

    .. seealso:: :ref:`pkg.bpt.yaml`

  project root

    The *project root* of a `project` is the `directory` that contains the
    project's |bpt.yaml| file.

A |bpt| `project` roughly corresponds to a source control repository and is the
directory that should be opened and modified with an IDE or text editor.


Understanding Packages
######################

In |bpt| the term "package" refers to a named+versioned collection of
`libraries <library>`. This can include a `project`, but often refers to some
pre-bundled set of files and directories that contains a |pkg.json| file. The
contents of |pkg.json| declare all of the properties required to consume a
package and the libraries it contains, but you won't often need to interact with
this file directly.

Packages are identified by a name/version pair, joined together by an ``@``
symbol, and with a `package revision number` appended. The version of a package
must be a :ref:`Semantic Version string <semver>`. Together, the
``name@version~revision`` string forms the `package ID`, and it must be unique
within a repository. The revision number can often be omitted.

If you are generating a package from a |bpt| `project` (using the
:ref:`cli.pkg-create` command), the |pkg.json| will be synthesized automatically
based on the content of the project's |bpt.yaml| file.

For this reason a "`project`" can be considered |bpt|'s "high-level" abstraction
of a `package`. A project is intended to be modified directly by an IDE or other
code editor, whereas a package is meant to be consumed by automated tools.


.. _pkg.bpt.yaml:

The Project |bpt.yaml| File
###########################

The |bpt.yaml| file in the root of a `project` is used to set the `package`
attributes of the project, including specifying the `libraries <library>` of the
project, as well as the `dependencies <dependency>` of those libraries.

In a |bpt.yaml| file, the only two required properties are :yaml:`name` and
:yaml:`version`::

  # Set the name of the project
  name: acme.widgets
  # Set the version of the project
  version: 4.1.2-dev

Refer to the `name` and :ref:`versions <semver>` documentation for information
about what makes a valid name and a valid version.

All other fields are described below.

.. default-domain:: schematic
.. rubric:: Schema
.. schematic:mapping:: Project

  This object defines the schema of the |bpt.yaml| file used to define a |bpt|
  `project`.

  .. property:: name
    :required:

    :type: string

    Specifies the `package name` of the `project`. Must fit the
    :term:`name rules <name>`.


  .. property:: version
    :required:

    :type: string

    Specifies the `package version` of the `project`. Must be a valid
    :ref:`Semantic Version string <semver>`.


  .. property:: dependencies
    :optional:

    :type: string[]

    Specify the `common dependencies` of the `project`. If provided, the value
    must be an array of `dependency specifier`\ s.

    .. seealso::

      - :doc:`deps`
      - :prop:`ProjectLibrary.dependencies`


  .. property:: test-dependencies
    :optional:

    :type: string[]

    Specify the `common test-dependencies <common dependencies>` of the
    `project`. If provided, the value must be an array of
    `dependency specifier`\ s.

    .. seealso::

      - :doc:`deps`
      - :prop:`ProjectLibrary.test-dependencies`


  .. property:: libraries
    :optional:

    :type: ProjectLibrary[]

    Specify the `libraries <library>` of the `project`. If omitted, |bpt| will
    generate a `default library` (recommended for most projects).

    .. seealso::

      - :ref:`proj.lib-properties`
      - :doc:`/guide/libraries`
      - :ref:`guide.default-library`
      - :ref:`guide.multiple-libs`
