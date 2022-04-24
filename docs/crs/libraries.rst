#########
Libraries
#########

.. |name| replace:: :doc:`name </crs/names>`
.. |package| replace:: :term:`package <CRS package>`
.. |library| replace:: :term:`library <CRS library>`
.. |libraries| replace:: :term:`libraries <CRS library>`
.. |dependencies| replace:: :term:`dependencies <CRS dependency>`
.. |dependency| replace:: :term:`dependency <CRS dependency>`

A CRS library is the smallest consumable unit in a |package|. A package can have
one or more libraries, but a library can only belong to one package. Each
library within a single package must have a unique |name|. A library can have
some number of |dependencies|, and can "use" other libraries within the same
package.

.. default-role:: term


Terms
#####

.. glossary::

  library name

    The name of the library. The name must be unique within the containing
    `CRS package`, and must be a valid |name|.

  library path

    The "path" of a library is a POSIX-style `relative filepath` (relative to
    the root of the containing package). The path must name a `CRS library root`
    within the package.

  library internal usages

    Each |library| in a |package| can declare that it "`uses <library usage>`"
    other libraries in that same |package|.

    .. seealso:: `Internal Usages`_

  library external dependencies

    Each library in a |package| can declare a unique set of |dependencies| and
    `library usages <library usage>` on libraries from other packages.

    .. seealso:: `External Dependencies`_

  library usage
    .. |def-library-usage| replace::

      Library usage is a one-way relationship between a
      `user library <library user>` and a `used library <used-library>`. The
      *user* library is able to reference and make use all entities publicly
      exposed by the *used* library, including headers, classes, functions,
      modules, and macros. Library usage also *transitive*.

    |def-library-usage|

    If :math:`A` uses :math:`B`, and :math:`B` uses :math:`C`, then :math:`A`
    has a *transitive* usage of :math:`C`.

    A `library usage` may identify a library within the same |package| as the
    `library user`, or may specify a library in a |dependency| package.

    When library :math:`A` uses library :math:`B`, :math:`B`\ 's
    `public header root` is added as a `header search path` for :math:`A`, and
    any programs in :math:`A` will be linked with the `public translation units`
    for :math:`B`.

    Library usage must never form a cycle where a library can (transitively)
    "use itself".

  library user

    The *users* of a |library| :math:`L` are the libraries that declare a
    `library usage` on :math:`L`.

    |def-library-usage|

  used-library

    The |libraries| *used-by* :math:`L` are the libraries upon which :math:`L`
    declares a `library usage`. :math:`L` becomes a "`library user`" of the
    libraries that it uses.

    |def-library-usage|

  CRS library root

    The `directory` in which the source files of a single |library| reside.
    Contains the ``src/`` and ``include/`` `directories <directory>` for a
    |library|.

    .. seealso:: :ref:`The documentation on library roots <crs.library.root>`

  CRS source root

    A subdirectories of the `CRS library root` that contain the source files for a
    the library (including headers).

    .. seealso:: `Source Roots`_

  source code

    The code for a program written in some programming language.

  source file

    Source files are regular (non-`directory`) files that contain code of some
    programming language.

  header
  header file

    A header file (or just "*header*") is kind of `source file` that contains
    `source code` that is not directly fed to a compiler. It is intended to be
    used within other source files via the |#include| preprocessor directive.

    Header files usually use a special `file extension` that indicates their
    being header files. Examples of header file extensions include ``.h`` and
    ``.hpp``

  public header root

    The subdirectory of the `CRS library root` that contains the header files
    that should be exposed to `library users <library usage>`.

    .. seealso:: `Source Roots`_

  private header root

    The subdirectory of the `CRS library root` that contains the header files
    that are required to compile the respective library, but *should not* be
    exposed to the `library users <library usage>`.

    It is possible that library does not have a private header root.

    .. seealso:: `Source Roots`_

  compiled source root

    The subdirectory of the `CRS library root` that contains the
    `source files <source file>` of the library that must be given to the
    compiler to generate the library's translation units.

    It is possible that a library does not have a compiled source root.

    .. seealso::

      - `Compiled Source Root`_
      - `Source Roots`_

  public translation units

    The set of translation units generated for a library that are included when
    linking downstream `library users <library usage>`.

    .. seealso:: `Collecting the Public Translation Units`_

    .. default-role:: math

  recognized compiled source file extension

    A :term:`file extension` that is defined to be a source file that generates
    a translation unit containing the definition of entities for a library.
    Comparing the file extension against the set of recognized extensions must
    be a case-insensitive comparison. Only the lowercase extensions are listed
    in this documentation.

    The source file extensions recognized by a tool will depend on the
    programming language under scrutiny.

    Only the ``.c`` file extension is supported for the C programming language.

    The following file extensions are available for the C++ programming
    language:

    - ``.cpp``
    - ``.cc``
    - ``.cxx``
    - ``.c++``

  header search path

    A :term:`filepath` from which a :term:`compiler` will resolve references to
    :term:`header files <header file>`.

    While the behavior of the |#include| directive is implementation-defined,
    CRS (and |bpt|) assumes the following behavior:

    1. When compiling a source file, the compiler has a list of
       :term:`header search path`\ s `L` that it will use to resolve |#include|
       directives.
    2. An |#include| or ``__has_include`` directive specifies a filepath `H` of
       the form ``<``\ `H`\ ``>``
    3. For each directory `L_s` in `L`:

       1. Create a filepath `H_s` by joining `L_s` and `H` with a directory
          separator.
       2. If `H_s` names a regular file, the directive will resolve to that
          file.

    4. If no directory in `L` was able to resolve `H`, the header is considered
       to be not-found.

.. default-role:: term

Properties
##########

The following are the salient attributes of a library within a package:

- `library name`
- `library path`
- `library internal usages`
- `library external dependencies`


.. _crs.library.root:

Roots
#####

The `CRS library root` is the directory in which the source files of a single
library reside. CRS recognizes two top-level subdirectories within a library
root: ``src/`` and ``include/``. A library root must contain one or both of
these directories. Each directory is a `CRS source root`. CRS does not impose any
semantics on any other files in the library root.

The following semantics apply, based on the presence of ``src/`` and/or
``include/``:

- If **both** ``src/`` and ``include/`` are present in the library root, then
  ``src/`` is the `private header root` and `compiled source root` for the
  library, while ``include/`` is the `public header root`.
- Otherwise, if ``src/`` is present, then ``src/`` is the `public header root`
  and the `compiled source root`. There is no `private header root`.
- Otherwise, ``include/`` must be present, and ``include/`` is the
  `public header root`. There is no `private header root` nor is there a
  `compiled source root`.

One should note that a library can only have two distinct source root paths. It
is not possible that the `public header root`, `private header root`, and
`compiled source root` are all pointing to distinct locations.


Source Roots
************

A *source root* is a directory that contains library source files, including
header files. If the source root is a `compiled source root`, then that
directory may contain source files that will be used to generate the consumable
translation units for the library. If a source root is not *compiled*, then it
is a *header-only* source root.

Both *compiled* and *header-only* source roots may contain headers, but whether
those headers can be resolved and used by downstream consumers depends on
whether the source root is the *public* header root or the *private* header
root.


Philosophy
==========

A source root should have the following properties:

- A source root is *relocatable*. No subdirectories are required to share this
  property.
- A source root is a `header search path`. No subdirectories should be
  `header search path`\ s.

Being *relocatable* in this context means that the location of the directory on
the filesystem is irrelevant to the code contained within. As long as the files
within a source root are compiled with their `used-libraries <library usage>`
available, moving or renaming the source root directory should never result in
any |#include| directives failing to resolve.

Code that explicitly assumes the location of its own source root, including in
relation to other source roots, is expressly unsupported by CRS.


.. rubric:: Example

Suppose we have a library with the following source files (note the file paths):

.. highlight:: cpp

.. code-block::
  :caption: ``include/mylib/foo.hpp``

  // I am a public header

.. code-block::
  :caption: ``src/mylib/bar.hpp``

  // I am a private header

.. code-block::
  :caption: ``src/mylib/foo.cpp``

  // [1] Okay:
  #include <mylib/foo.hpp>

  // [2] Not okay
  #include <foo.hpp>

  // [3] Not okay
  #include "../include/foo.hpp"

  // [4] Okay and recommended
  #include <mylib/bar.hpp>

  // [5] Okay and recommended
  #include "./bar.hpp"

  // [6] Okay, but suspicious
  #include "bar.hpp"
  #include <bar.hpp>


This library has two `CRS source root` directories: ``include/`` and ``src/``.
``src/`` is the `private header root` and `compiled source root`, while
``include/`` is the `public header root`. The following applies to each example:

1. **Okay**: ``<mylib/foo.hpp>`` is a fully-qualified path from the
   `public header root` directory, which will be set as a `header search path`
   while compiling the file.

2. **Not okay**: The ``<foo.hpp>`` directive will not resolve because that path
   will not name an existing file when joined with the any `header search path`.

3. **Not okay:** This will only work so long as the ``include/`` and ``src/``
   directories are siblings of the same parent directory, but this violates the
   restriction that these directories be *relocatable*.

4. **Okay and recommended**: Like ``[1]``, specifies a fully-qualified path from
   the `private header root` directory, which will be added as a
   `header search path` while compiling the file.

5. **Okay**: Relative |#include| directives that specify a leading ``.`` or
   ``..`` are unambiguous to the reader, although it is possible that they will
   still resolve within a different `header search path` than the containing
   file's own directory.

6. **Okay, but suspicious**: While most compilers will insert the containing
   directory of the file being compiled as a `header search path`, it is not
   clear that the author is relying on this fact without the reader knowing that
   ``bar.hpp`` names a file within the same directory.


Public Header Root
******************

The `public header root` is the `CRS source root` directory that contains
headers that will be visible to the `library's users <library user>`. When
compiling the containing library and all of its users, the `public header root`
of the library should be added as a `header search path`.

Transitive usage also applies: If library :math:`A` uses library :math:`B`, and
library :math:`B` uses library :math:`C`, the `public header root` of :math:`C`
should be included as a `header search path` while compiling :math:`A`, even if
:math:`A` does not have an explicit `library usage` on :math:`C`.


Private Header Root
*******************

The `private header root` is the `CRS source root` directory that contains
headers that should not be visible to the `library's users <library user>`, but
*will* be visible while compiling the library's `public translation units`.

When compiling the `public translation units`, the `private header root` will be
added as a `header search path`.

None of the header files in the library's `public header root` should refer to
the private headers. A verification step by compiling/checking the public
headers *should not* add the `private header root` as a `header search path`.


Compiled Source Root
********************

The `compiled source root` is the source root directory that contains the files
that will be used to generate the library's :term:`public translation units`.


Collecting the Public Translation Units
=======================================

The `public translation units` of a |library| are given by finding the *public
compiled sources* of the library.

For each file :math:`S` in the `compiled source root` `directory` and all descendent
directories, with a `filepath` :math:`F_s` as a `relative filepath` from its
containing source root:

1. If the `file extension` of the `file stem` of :math:`F_s` is "``.test``" or
   "``.main``", then :math:`S` *is not* a public compiled source.
2. Otherwise, if the `file extension` of :math:`F_s` is a
   `recognized compiled source file extension`, then :math:`S` *is* a public
   compile source.
3. Otherwise, :math:`S` *is not* a public compiled source.

When linking a program :math:`P` that `uses <library usage>` a |library|
:math:`L`, every public translation unit of :math:`L` should be included in the
link operation. The linker *may* discard translation units which would be
unnecessary to generate :math:`P`.


Internal Usages
###############

Libraries within a single `CRS package`


.. _crs.library.dependencies:

External Dependencies
#####################

Dependencies
