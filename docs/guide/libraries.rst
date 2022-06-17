Libraries in |bpt|
##################

.. default-role:: term

In |bpt|, all code belongs to a `library`, and every library belongs by either a
|bpt| `project` or a `package`.

By default, a |bpt| project only has a single `default library`. A project can
be subdivided into more libraries by using the |prop:bpt.yaml:libraries|
property. A library can `depend on <dependency>` other libraries in external
packages.

|bpt| uses the filesystem layout of the library to derive information about it.
Every library has a `library root` directory. A library root contains either
*one* or *two* `source roots <source root>` (``src/`` and/or ``include/``).

Libraries within `projects <project>` are defined using |bpt.yaml|. There may be
an implicit `default library`, or some number of
:ref:`explicitly declared libraries <guide.multiple-libs>` using the
:yaml:`libs` key of |bpt.yaml|.

The project's |bpt.yaml| defines the `library roots <library root>` of every
library in the project:

- If relying on the `default library`, the `library root` is the same as the
  `project root` of the project that contains it. This is the recommended usage
  mode for most projects.
- If using :ref:`explicitly declared libraries <guide.multiple-libs>`, the
  `library root` is defined by a relative path specified using the :yaml:`path`
  property of the library declaration in |bpt.yaml| (Refer:
  :ref:`proj.lib-properties`).

Every library root must contain a ``src/`` directory, *or* an ``include/``
directory, *or both*. |bpt| will treat both directories as
`source roots <source root>`, but behaves differently between the two.

A single `library root` will always correspond to exactly one library. If the
library has any compilable source files then |bpt| will use those sources to
generate a static library file that is linked into runtime binaries. If a
library contains only headers (a `header-only library`) then |bpt| will not
generate an archive to be included in downstream binaries, but it will still
generate link rules for the dependencies of a header-only library.


.. _guide.default-library:

The Default Library
*******************

In |bpt|, all code belongs to a `library`. If a |bpt.yaml| file omits the
|prop:bpt.yaml:libraries| property, |bpt| will assume that the `project`'s
`default library` lives in the same directory as |bpt.yaml| and has a library
:yaml:`name` equivalent to the project :yaml:`name`:

.. code-block:: yaml
  :caption: |bpt.yaml|

  name: acme.widgets
  version: 1.2.3

  # ┌ Implied: ──────────────┐
    │ libraries:             │
    │   - name: acme.widgets │
    │     path: .            │
  # └────────────────────────┘

The above project definition implies a single default library with the same name
as the project itself: "``acme.widgets``". The `library root` of the default
library is always the same as the `project root`, and cannot be changed.

.. seealso::

  The |prop:bpt.yaml:proj.libraries| property allows one to specify any number
  of libraries within the project. The :yaml:`libraries` property is discussed
  below: :ref:`guide.multiple-libs`

.. note::

  If your project only defines a single library, you are likely to not need to
  use |prop:bpt.yaml:proj.libraries| and can just rely on the implicit default
  library.

.. note::

  If the |prop:bpt.yaml:proj.libraries| property is specified then |bpt| will
  not generate a default library.


.. _guide.multiple-libs:

Multiple Libraries in a Project
*******************************

Multiple libraries can be specified for a single `project` by using the
|prop:bpt.yaml:proj.libraries| property in |bpt.yaml|. :yaml:`libraries` must be
an array, and each element must be a map, and each map element must have both a
:yaml:`name` and a :yaml:`path` property:

.. code-block::
  :caption: |bpt.yaml|
  :emphasize-lines: 4-6

  name: acme.widgets
  version: 1.2.3

  libraries:
    - name: gadgets       # Required
      path: libs/gadgets  # Required

.. seealso::

  For more information on using the :yaml:`libraries` array, refer to:
  :ref:`proj.lib-properties`.


.. _libs.library-layout:

Library Layout
**************

The layout expected by |bpt| is based on the `Pitchfork layout`_ (PFL). |bpt|
does not make use of every provision of the layout document, but the features it
does have are based on PFL.

.. _Pitchfork layout: https://api.csswg.org/bikeshed/?force=1&url=https://raw.githubusercontent.com/vector-of-bool/pitchfork/develop/data/spec.bs

In particular, the ``src/`` and ``include/`` directories are used are used as
`source roots <source root>`.

The smallest subdivision of code that |bpt| recognizes is the `source file`,
which is exactly as it sounds: A single file containing some amount of
`source code`.

Source files can be grouped on a few axes, the most fundamental of which is
"Is this compiled?"

|bpt| uses source `file extensions <file extension>` to determine whether a
source file should be fed to A `compiler`. All of the common C and C++ file
extensions are supported:

.. list-table::

    - * Compiled as C
      * ``.c``

    - * Compiled as C++
      * ``.cpp``, ``.c++``, ``.cc``, and ``.cxx``

    - * Checked but not compiled (`header files <header file>`)
      * ``.h``, ``.h++``, ``.hh``, ``.hpp``, and ``.hxx``

    - * Not checked or compiled
      * ``.ipp``, ``.inc``, and ``.inl``

If a file's extension is not listed in the table above, |bpt| will ignore it.
File extensions are case-insensitive for the purpose of this lookup.

.. note::

  Although headers are not compiled, this does not mean they are ignored. For
  regular `header files <header file>`, |bpt| performs a "compilability check"
  on them to ensure that they can be used in isolation. Un-checked
  "include-files" such as ``.ipp``, ``.inc``, and ``.inl`` are not checked, but
  they are collected together as part of the `package` for distribution.


.. _guide.source-roots:

Source Roots
************

.. glossary::

`Source files <source file>` are collected as descendents of some *source root*
`directory`. A *source root* is a single directory that contains some *portable*
bundle of source files. The word "portable" is important: It is what
distinguishes the source root from its child directories.


Portability
===========

By saying that a source root is "portable", we mean that the `directory` itself
can be moved, renamed, or copied without breaking the |#include| directives of
its children or of the directory's referrers.

As a practical example, let's examine such a directory, which we'll call
``src/`` for the purposes of this example. Suppose we have a such a directory
with the following structure:

.. code-block:: text

    <path>/src/
      animals/
        mammal/
          mammal.hpp
        cat/
          cat.hpp
          sound.hpp
          sound.cpp
        dog/
          dog.hpp
          sound.hpp
          sound.cpp

In this example, ``src/`` is a *source root*, but ``src/animals/``,
``src/animals/cat/``, and ``src/animals/dog/`` are **not** source roots. While
they may be directories that contain `source files <source file>`, they are not
"roots."

Suppose now that ``dog.hpp`` contains an |#include| directive:

.. code-block:: c++

    #include <animals/mammal/mammal.hpp>

or even a third-party user that wants to use this `library`:

.. code-block:: c++

    #include <animals/dog/dog.hpp>
    #include <animals/dog/sound.hpp>

In order for any code to compile and resolve these |#include| directives, the
``src/`` directory must be added as a `header search path`.

Because the |#include| directives are based on the `source root`, and the source
root is *portable*, the exact `filepath` location of the source root directory
is not important to the content of the consuming source code, and can thus be
relocated and renamed as necessary. Consumers only need to update the content of
their `header search path`\ s in a single location rather than modifying their
source code.

.. note::

  |bpt| manages header search paths automatically.


.. _libs.source-kinds:

Source Root Kinds
=================

The naming or `source roots <source root>` determines how |bpt| will treat the
`source files <source file>` in that directory. A source root can be *compiled*
or *header-only*, and *public* or *private*.

|bpt| distinguishes between a library's *public* `source root`, and a *private*
source root. The `headers <header file>` within the *private* source root are
`private headers` of the library, and the headers within the *public* source
root are the `public headers` of the library.

When |bpt| is compiling a `library`, both its *private* and its *public* source
roots will be added to the compiler's list of `header search path`\ s. This
allows that library to freely refer to both its *public* and *private* headers.

On the other hande, when a downstream user of a library :math:`L` is being
compiled, only the *public* source root of that library :math:`L` will be added
as a header search path. This restricts downstream libraries to only have access
to the `public headers` of the libraries that it uses.

|bpt| supports either one or two source roots in a library. Their naming
determines which directories are treated as *public* or *private*.

.. rubric:: The Compiled Source Root: ``src/``

If the `library root` contains a ``src/`` `directory`, then |bpt| will treat
that directory as the *compiled* `source root`, and will compile any files
within that directory that have an appropriate `file extension`. While compiling
those files, the ``src/`` directory will be given as a `header search path` for
resolving |#include| directives.

.. note::

  One can always safely place header files in ``src/``.

.. rubric:: The *Header-Only* Source Root: ``include/``

If the library root contains an ``include/`` `directory`, then that directory
will be treated as the `header`-only `source root`. No files within this
directory will be compiled, but |bpt| will still validate that every file that
has an appropriate `header` `file extension` could be passed to the compiler on
its own.

.. rubric:: Public vs. Private Source Roots

If **both** ``src/`` and ``include/`` are present in a `library root`, then
``src/`` will be treated as the *private* `source root` and ``include/`` will be
treated as the *public* source root. Users of the `library` will be able to
resolve the headers in the ``include/`` directory (they are `public headers`),
but not headers in the ``src/`` directory (which are `private headers`).
Additionally: Header files in the ``include/`` directory **will not** be able to
reference any of the private headers in ``src/``, but private headers in
``src/`` will always be able to reference public headers in ``include/``.

.. warning::

  Because only the `public headers` are available when compiling library
  consumers, it is essential that no headers within the *public* source root
  attempt to use `private headers` as they **will not be visible**.

If **only one of** ``src/`` *or* ``include/`` is present in the `library root`,
that directory will be treated as the public `header` root for the library, and
users will be able to |#include| all headers in the library. There will be no
*private* header root.

If **only** ``include/`` (**and not** ``src/``) is present in the
`library root`, then |bpt| will treat it as a `header-only library` (No
`source files <source file>` in ``include/`` will be compiled).

When |bpt| exports a `library` to a `package`, the `header files <header file>`
from the *public* source root will be collected together and distributed as that
library's header tree. The path to the individual header files relative to their
source root will be retained as part of the library distribution.

By default, |bpt| will compile *all* compilable `source files <source file>`
that appears in the ``src/`` directory. |bpt| will not compile compilable source
files that appear in the ``include/`` directory and will issue a warning if any
such files are found.

.. note::

  Some source files will be treated differently based on there name. Refer:

  - :doc:`apps`


.. _proj.lib-properties:

Library Properties in |bpt.yaml|
********************************

A |bpt| `project` can declare one or more `libraries <library>` using the
top-level |prop:bpt.yaml:proj.libraries| property in |bpt.yaml|. If the
:yaml:`libraries` property is omitted, |bpt| will instead generate a
`default library` for the project. Most projects will not need to declare
explicit :yaml:`libraries` and can rely on the default library.

The project's :yaml:`libraries` must be an array, and each element must be a
:schematic:mapping:`ProjectLibrary` map. The only required properties are
:yaml:`name` and :yaml:`path`:

.. code-block:: yaml
  :caption: |bpt.yaml|

  name: my-project
  version: 1.2.3

  libraries:
    # Declare one library "my-library"
    - name: my-library
      path: libs/mylib
    # Declare a second library
    - name: widgets
      path: libs/widgets

Refer to [`YAML`] for a quick-start on the YAML syntax. If nothing else, you can
use YAML's flow-syntax as an "enhanced `JSON`" that supports :yaml:`# comments`
and unquoted identifier keys::

  {
    name: "my-project",
    version: "1.2.3",
    libraries: [
      {name: "my-library", path: "libs/mylib"},
      {name: "widgets", path: "libs/widgets"},
    ]
  }


.. note::

  For the `default library` (if |prop:bpt.yaml:proj.libraries| is omitted), the
  :yaml:`name` is the same as the project's name, and the :yaml:`path` is
  "``.``" (equivalent to the `project root`).


.. default-domain:: schematic

.. rubric:: Schema

.. mapping:: ProjectLibrary

  A mapping in |bpt.yaml| that defines a `library` for the `project`. These
  mapping objects appear as elements of the :prop:`~Project.libraries` array for
  the project.

  .. code-block:: yaml
    :caption: |bpt.yaml|
    :emphasize-lines: 9-11, 13-15

    name: acme-components
    version: 4.1.2

    # Every library will share the common test-dependencies:
    test-dependencies: [catch2@3.0.1]

    # This project defines two libraries listed below:
    libraries:
      - name: util
        path: libs/util
        depends: [fmt@8.1.3, asio@1.12.0]

      - name: gui
        path: libs/gui
        using: [util]  # 'gui' uses 'util'


  .. property:: name
    :required:

    :type: string

    The name of the library. Must follow the valid `name` conventions. This name
    must be unique within the `project`.


  .. property:: path
    :required:

    :type: string

    The :yaml:`path` property specifies the `relative filepath` pointing to the
    `library root` for the library. This path must be relative to the
    `project root` (the directory that contains |bpt.yaml|) and may only use
    forward-slash "``/``" as a `directory separator`. The :yaml:`path` must not
    "reach outside" of the project root directory. A path of a single ASCII dot
    "``.``" refers to the project root itself.


  .. property:: using
    :optional:

    :type: string[]

    The internal `dependencies <dependency>` of the library. Must be
    the names of other libraries within the same `project`.

    .. code-block:: yaml
      :emphasize-lines: 10-12

      name: acme-items
      version: 1.2.3

      libraries:
        - name: widgets
          path: libs/widgets

        - name: gadgets
          path: libs/gadgets
          using: [widgets]  # All entities from widgets will be
                            # available to gadgets, as well as to
                            # any of gadgets' dependents


  .. property:: test-using
    :optional:

    :type: string[]

    The internal `test dependencies <test dependency>` of the library. Must be
    the names of other libraries within the same `project`. Like :prop:`using`,
    but these dependencies are only used for building tests.


  .. property:: dependencies
    :optional:

    :type: string[]

    The external `dependencies <dependency>` of the library. If provided, the
    value must be an array of `dependency specifier`\ s.

    The dependencies here will be merged with the `common dependencies` of the
    `project` (from :prop:`Project.dependencies`).


  .. property:: test-dependencies
    :optional:

    :type: string[]

    The external `dependencies <dependency>` of the library. If provided, the
    value must be an array of `dependency specifier`\ s. Like
    :prop:`dependencies`, but these dependencies are only used for building
    tests.

    The dependencies here will be merged with the
    `common test dependencies <common dependencies>` of the `project` (from
    :prop:`Project.test-dependencies`.
