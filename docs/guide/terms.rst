#########################
Miscellaneous Terminology
#########################

.. default-role:: term

This page documents miscellaneous terminology used throughout |bpt| and `CRS`.

.. glossary::
  :sorted:

  JSON

    JSON is the *JavaScript Object Notation*, a plaintext format for
    representing semi-structured data.

    JSON values can be strings, numbers, boolean values, a ``null`` value,
    arrays of values, and "objects" which map a string to another JSON value.

    .. rubric:: Example

    .. code-block:: json
      :caption: ``example.json``

      {
        "foo": 123.0,
        "bar": [true, false, null],
        "baz": "quux",
        "another": {
          "nested-object": [],
        }
      }

    JSON data cannot contain comments. The order of keys within a JSON object is
    not significant.


  YAML

    YAML is a data representation format intended to be written by humans and
    contain arbitrarily nested data structures.

    YAML is a superset of `JSON`, so every valid JSON document is also a valid
    equivalent YAML document.

    You can learn more about YAML at https://yaml.org.

    The example from the `JSON` definition could also be written in YAML as:

    .. code-block:: yaml
      :caption: ``example.yaml``

      foo: 123.0
      bar:
        - true
        - false
        - null
      # We can use comments in YAML!
      baz: quux
      another:
        nested-object: []

    YAML can also be written as a "better JSON" by using the data block
    delimiters:

    .. code-block:: yaml
      :caption: ``example.yaml``

      {
        foo: 123.0,
        bar: [true, false, null],
        # We can still use comments!
        baz: "quux",
        another: {nested-object: []},  # Trailing commas are allowed!
      }

    .. note:: Any `JSON` document is also a valid equivalent YAML document.

    .. note::

      |bpt| parses according to YAML 1.2, so alternate boolean plain scalars
      (e.g. "``no``" and "``yes``") and sexagesimal datetimes are not supported.

  tweak-headers

    Special `header files <header file>` that are used to inject configuration
    options into dependency libraries. The library to be configured must be
    written to support tweak-headers.

    Tweak headers are placed in the "tweaks directory", which is controlled with
    the :option:`bpt build --tweaks-dir <--tweaks-dir>` option given to |bpt|
    build commands.

    .. seealso:: For more information, `refer to this article`__.

    __ https://vector-of-bool.github.io/2020/10/04/lib-configuration.html

  URL

    A **U**\ niform **R**\ esource **L**\ ocator is a string that specifies how to
    find a resource, either on the network/internet or on the local filesystem.

  environment variables

    Every operating system process has a set of *environment variables*, which
    is an array of key-value pairs that map a text string key to some text
    string value. These are commonly used to control the behavior of commands
    and subprocesses.

    For example, the "``PATH``" environment variable controls how `command`
    names are mapped to executable files.

    |bpt| uses some environment variables to control some behavior, such as
    :envvar:`BPT_LOG_LEVEL` and :envvar:`BPT_NO_DEFAULT_REPO`.

  application

    An *application* is a program that is intended to be run and distributed to
    users to perform some set of designated tasks.

    In |bpt|, an application executable is created for each `source file` with
    an appropriate `file stem`.

    .. seealso:: :doc:`apps`

  test

    In |bpt|, "test" refers to a `library`-provided execuatble program that can
    be used to verify that the library implements the correct behavior.

    Tests are compiled and linked automatically as part of any
    :doc:`/guide/cli/build` invocation.

    .. seealso:: :doc:`tests`

  compiler
  compile

    *Compiling* is the process of transforming human-readable `source code` and
    emits a lower-level code intended to be executed. The *compiler* is a
    program that performs the compilation. GCC, Visual C++, and Clang are
    examples of *compilers*.

  linker
  linking

    *Linking* is the process of combining separate translation units (i.e.
    compiled source files code) into a program. A *linker* is a program that
    performs linking.

    During linking, references to names across translation units are resolved.
    If a name is referenced but its definition is not found, the linker will
    most often fail to perform the linking.

  default library

    The *default library* of a `project` is the `library` that |bpt| generates
    if the :yaml:`libraries` (:schematic:prop:`Project.libraries`) property is
    omitted in |bpt.yaml|. It will have the same :yaml:`name` as the project,
    and its `library root` will be the same as the `project root`.

    .. note::

      If the :yaml:`libraries` property is specified then |bpt| will not
      generate a default library.

    .. seealso::

      - :ref:`guide.default-library`
      - :ref:`guide.multiple-libs`

  header-only library

    A *header-only library* is a `library` that contains no exported compilable
    source files, and only contains `header files <header file>`.


  library root

    The `directory` in which the source files of a single `library` reside.
    Contains the ``src/`` and/or ``include/`` directories for that library, each
    of which is a `source root`.

    This path is specified using the :yaml:`libs[].path` key of the library in
    |bpt.yaml|.

    .. seealso::

      - :doc:`libraries`
      - :ref:`libs.library-layout`
      - `CRS library root`

  source root

    A `directory` within a `library` that defines the structure of a source tree
    and and acts as a `header search path`. This directory contains
    `source files <source file>`.

    Within a `library root`, the ``src/`` and ``include/`` directories are
    source roots.

    .. seealso::

      - :ref:`guide.source-roots`
      - :ref:`libs.library-layout`
      - :ref:`libs.source-kinds`
      - :term:`CRS source root`

  public headers
  private headers

    Within a `library`, the *public* headers and *private* headers of are the
    `header files <header file>` that live in the *public* `source root` or the
    *private* source root, respectively.

    The *public* headers are visible to the library's users, but the *private*
    headers are only available to the library while compiling the library
    itself.

    .. seealso:: :ref:`libs.source-kinds`

  library

    Within the context of software development, a *library* is a set of code
    that is designed to be used by other code to build
    `applications <application>` or additional higher-level libraries.

    A library contains a set of definitions of entities that can be used by
    other code. This facilitates code reuse.

    A library is the smallest consumable unit of code. That is: You cannot "use"
    only a subset of a library when building a library or application.

    .. seealso::

      - :doc:`libraries`
      - :term:`CRS library`

  test dependency

    A *test dependency* is a `dependency` within a `library` that is only used
    for compiling and linking the library's `tests <test>`.

    Unlike regular dependencies, test dependencies are **not** *transitive*.

    Test dependencies are declared using the :yaml:`test-dependencies` property
    in |bpt.yaml|.

  common dependencies

    The *common dependencies* of a `project` are the `dependencies <dependency>`
    that appear at the top-level of the project's |bpt.yaml| file. (Refer:
    :schematic:prop:`Project.dependencies`)

    These dependencies are added as direct dependencies of every `library` in
    the project, whether that is the `default library` or each library in the
    :schematic:prop:`Project.libraries` array.

    The *common test-dependencies* are the similar but apply only to the
    libraries' `test dependencies <test dependency>`. (Refer:
    :schematic:prop:`Project.test-dependencies`)

  package ID

    A *package ID* is a string that identifies a package. It is composed of the
    package's `name`, :ref:`version <semver>`, and the
    `package revision number`:

      ``<name>@<version>~<revision>``

  package revision number

    `CRS` allows packages within a repository to be updated without changing the
    :ref:`version <semver>` of the package itself. This is reserved for changes
    that only update the package metadata or fix critical issues that render
    a prior revision to be unusable. The revision number is always a single
    positive integer and begins at ``1``.

    The package revision number is visible on the `package ID` as the number
    following the tilde ``~`` suffix.

  dependency specifier

    |bpt| allows a few syntaxes to specify a `dependency`. A specifier provides
    a `name`, a :ref:`version range <semver>`, and some set of `library` names
    to use from the external `package`.

    .. seealso:: :ref:`dep-spec`

  CMake

    **CMake** is a popular cross-platform build system and project configuration
    tool for C and C++ projects.

    .. seealso::

      - |bpt| has support for integrating with CMake

        -  :doc:`/guide/cmake`
        -  :doc:`/howto/cmake`

      - `The CMake homepage <https://cmake.org/>`_
