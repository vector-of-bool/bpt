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
    many purposes, |bpt| will treat a project as a `package` with special
    properties.

    |bpt| can capture a project directory as a `package` for distribution and
    use in other tools by using the :ref:`cli.pkg-create` subcommand.

    .. seealso:: :ref:`guide.projects`

  package

    A *package* is a `named <name>` and :ref:`versioned <semver>` collection of
    `libraries <library>` distributed as a unit, available to be "used" to build
    additional libraries or `applications <application>`.

    A package contains a |crs.json| file (whereas a `project` would contain a
    |bpt.yaml| file).

    When |bpt| is building a dependency solution from some set of dependency
    statements, the `name` of the packages are used to create uniqueness: For
    every package in a dependency solution, each `name` will only resolve to
    only a single version.

    .. seealso:: :term:`CRS package`

  dependency

    A *dependency* specifies a requirement on `libraries <library>` in an
    external `package` in order to build and use the library that has the
    dependency.

    A dependency specifies the `name` of an external package, a range of
    compatible :ref:`versions <semver>`, and a list of one or more libraries
    from the depended-on package that are required.

    In |bpt| (and `CRS`), dependencies are attached to individual
    `libraries <library>`, and not to the `package` that contains that library.

    .. seealso:: :term:`CRS dependency`


.. _guide.projects:

Understanding Projects
######################

When using |bpt|, one is most often working within the scope of a `project`. A
`directory` is a |bpt| *project root* if it contains a ``bpt.yaml``:

.. glossary::

  ``bpt.yaml``

    The ``bpt.yaml`` file is placed in the root `directory` of a `project`. It
    defines the `package` attributes of the project, including the `name`,
    :ref:`version <semver>`, `libraries <library>`, and the
    `dependencies <dependency>` of those libraries.

    .. rubric:: Example

    .. code-block:: yaml
      :linenos:

      # Required: The name of the project
      name: my-example-package
      # Required: The version of the project
      version: 2.5.1-dev

      # Optional: Dependencies
      dependencies:
        - boost@1.77.0 using asio, filesystem

  project root

    The *project root* of a project is the `directory` that contains the
    project's |bpt.yaml| file.

A |bpt| `project` roughly corresponds to a source control repository and is the
directory that should be opened and modified with an IDE or text editor.

Within a |bpt.yaml| file, only the :yaml:`name` and :yaml:`version` keys are
required.


.. _guide.default-library:

The Default Library
*******************

In |bpt|, all code belongs to a `library`. If a |bpt.yaml| file omits the
:yaml:`libs` property, |bpt| will assume that the `project`'s `default library`
lives in the same directory as |bpt.yaml| and has a library :yaml:`name`
equivalent to the project :yaml:`name`:

.. code-block:: yaml
  :caption: |bpt.yaml|

  name: acme.widgets
  version: 1.2.3

  # ┌ Implied: ──────────────┐
    │ libs:                  │
    │   - name: acme.widgets │
    │     path: .            │
  # └────────────────────────┘


The above project definition implies a single default library with the same name
as the project itself: "``acme.widgets``". The `library root` of the default
library is always the same as the `project root`, and cannot be changed.

.. seealso::

  The :yaml:`libs` property allows one to specify any number of libraries within
  the project. The :yaml:`libs` property is discussed below:
  :ref:`guide.multiple-libs`

.. note::

  If your project only defines a single library, you are likely to not need to
  use :yaml:`libs` and can just rely on the implicit default library.

.. note::

  If the :yaml:`libs` property is specified then |bpt| will not generate a
  `default library`.


.. _guide.multiple-libs:

Multiple Libraries in a Project
*******************************

Multiple libraries can be specified for a single `project` by using the
top-level :yaml:`libs` property in |bpt.yaml|. :yaml:`libs` must be an array,
and each element must be a map, and each map element must have both a
:yaml:`name` and a :yaml:`path` property:

.. code-block::
  :caption: |bpt.yaml|
  :emphasize-lines: 4-6

  name: acme.widgets
  version: 1.2.3

  libs:
    - name: gadgets       # Required
      path: libs/gadgets  # Required

Refer to [`YAML`] for a quick-start on the YAML syntax. If nothing else, you can
use YAML's flow-syntax as an "enhanced `JSON`" that supports :yaml:`# comments`
and unquoted identifier keys::

  {
    name: "acme.widgets",
    version: "1.2.3",
    libs: [
      {name: "gadgets", path: "libs/gadgets"},  # Both required
    ]
  }

The :yaml:`path` property specifies the `relative filepath` pointing to the
`library root` for the library. This path must be relative to the `project
root` and may only use forward-slash "``/``" as a `directory
separator`. The :yaml:`path` must not "reach outside" of the `project root`. A
path of a single ASCII dot "``.``" refers to the project root itself (This is
the path of the `default library` if :yaml:`libs` is omitted).


Understanding Packages
######################

In |bpt| the term "package" refers to a named+versioned collection of
`libraries <library>`. This can include a `project`, but often refers to some
pre-bundled set of files and directories that contains a |crs.json| file. The
contents of |crs.json| declare all of the properties required to consume a
package and the libraries it contains, but you won't often need to interact with
this file directly.

Packages are identified by a name/version pair, joined together by an ``@``
symbol, and with a `package revision number` appended. The version of a package
must be a :ref:`Semantic Version string <semver>`. Together, the
``name@version~revision`` string forms the *package ID*, and it must be unique
within a repository. The revision number can often be omitted.

If you are generating a package from a |bpt| `project` (using the
:ref:`cli.pkg-create` command), the |crs.json| will be synthesized automatically
based on the content of the project's |bpt.yaml| file.

For this reason a "`project`" can be considered |bpt|'s "high-level" abstraction
of a `package`. A project is intended to be modified directly by an IDE or other
code editor, whereas a package is meant to be consumed by automated tools.
