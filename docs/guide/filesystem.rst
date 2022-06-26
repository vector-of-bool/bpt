######################
Filesystem Terminology
######################

This page lists various terminology used by |bpt| and :doc:`CRS </crs/index>`
for filesystem paths and manipulation.


.. default-role:: term

.. glossary::

  directory

    A directory is a filesystem object that can contain other files and
    directories. These are sometimes called "folders," as it is common to be
    represented with a folder icon.

  subdirectory

    The term "subdirectory" simply refers to a `directory` that is the child of
    another `directory`.

  working directory

    Every process on an operating system has a single "working directory"
    `filepath`. This is usually inherited from the parent process that started
    the child process.

    In a command-line environment, the working directory can be controlled with
    the ``cd`` command.

  path
  filepath

    A filepath (sometimes just "path") is a text string that represents a
    location on the filesystem. A filepath may be a `relative filepath` or an
    `absolute filepath`.

  absolute filepath

    An *absolute* filepath is a `filepath` that identifies a unique and
    unambiguous location on a filesystem. Any filepath that is not an *absolute*
    filepath is a `relative filepath`.

    On Unix-like systems, a filepath that begins with a `directory separator` is
    an absolute filepath, e.g. ``/foo/bar`` is *absolute*, while ``foo/bar`` is
    *relative*.

    On Windows, an absolute path requires a *root name* (e.g. a drive letter)
    followed by a `directory separator`. A path such as ``/foo/bar`` is
    *drive-relative* on Windows. One can resolve a drive-relative path to an
    absolute path by prepending a drive letter, such as ``C:`` in
    ``C:/foo/bar``. Windows applications will often accept ``/foo/bar`` in place
    of absolute path, since it is often unambiguous with context what the path
    identifies.

  relative filepath

    A *relative* filepath is a `filepath` that identifies a filesystem location
    "relative" to some other filepath. Any filepath that is not an
    `absolute filepath` is a relative filepath.

    "Resolving" a relative filepath is done by concatenating some base path with
    the relative filepath, joined by a `directory separator`.

    For example, given a relative path ``foo/bar`` and a Unix-like base absolute
    path ``/some/directory``, the relative path can be resolved by appending a
    `directory separator` to the base path followed by the relative path,
    resulting in the new absolute path ``/some/directory/foo/bar``.

  directory separator

    A character used within a `filepath` to delineate the components of that
    path.

    In a filepath such as ``foo/bar``, "``foo``" would be a directory that
    contains a file named ``bar``. The "``/``" represents the "parent path of"
    relationship.

    On Unix-like systems, the directory separator is a forward-slash "``/``". On
    Windows, both the forward-slash *and* the back-slash "``\``" are valid as
    directory separators.

    Multiple adjacent directory separators in a `filepath` are equivalent to a
    single directory separator. e.g. ``foo//bar////baz.txt`` identifies the same
    location as ``foo/bar/baz.txt``.

  filename

    A *filename* is the right-most component of a `filepath`, with any parent
    directory path removed. Any filename can also be considered a
    `relative filepath` with a single path component.

    .. default-role:: math

    Given a :term:`filepath` string `P`, the *filename* of `P` is the substring
    of `P` beginning immediately after the final :term:`directory separator`
    until the ened of the string, with the following points to consider:

    - If `P` is an empty string, the filename is an ASCII dot "``.``"
    - If `P` ends with a :term:`directory separator`, the filename is an ASCII
      dot "``.``"
    - If `P` does not contain a directory separator, the entire string `P` is
      the filename itself.

  file extension

    The *file extension* of a :term:`filepath` is the substring of it's
    :term:`filename` beginning at the final ASCII dot "``.``" until the end of
    the string, unless the first character of the :term:`filename` is the final
    ASCII dot "``.``".

    .. csv-table:: Examples
      :align: center
      :widths: auto

      .. default-role:: literal

      :term:`Filepath`, File extension, Explanation
      `baz.txt`, `.txt`,
      `bar.txt.gz`, `.gz`, Only the right-most extension is considered.
      `foo/bar.tar.gz`, `.gz`, The parent directory path is irrelevant.
      `foo.dir/bar.tgz`, `.tgz`, The parent directory path is irrelevant.
      `some_file`, (empty string), There is no dot in the :term:`filename`
      `foo.dir/some_file`, (empty string), There is no dot in the :term:`filename`
      `file`, (Empty string)
      `foo/bar/.`, (Empty string)
      `foo/bar/..`, (Empty string)
      `.hidden.txt`, `.txt`
      `.hidden`, (Empty string), The initial dot is not part of the extension

    .. default-role:: math

    .. rubric:: File extension algorithm

    Given a filepath `P`, with the :term:`filename` of `P` as `F`, then the file
    extension `E` of `P` is calculated as:

    1. If `F` begins with an ASCII dot "``.``", remove it.
    2. If `F` is a single ASCII ddot "``.``", then `E` is an empty string.
    3. Otherwise, if `F` does not contain an ASCII dot "``.``", `E` is an empty
       string.
    4. Otherwise, `E` is the substring of `F` beginning at (and including) the
       last ASCII dot "``.``" in `F` until the end of `F`

  file stem

    The *file stem* of a filepath `P` is the :term:`filename` of `P` with the
    :term:`file extension` removed.

    .. note::

      When obtaining the file stem of a string, only the outer-most
      :term:`file extension` should be removed.

    .. csv-table::
      :align: center
      :widths: auto

      .. default-role:: literal
      :term:`Filepath`, File stem, Explanation
      `baz.txt`, `baz`, Removed `.txt`
      `/foo/bar/baz.txt`, `baz`, Removed the parent directory path and `.txt`
      `file.tar.gz`, `file.tar`, Only the final `.gz` is removed.
      `file`, `file`, "No extension, so the :term:`filename` is the file stem"
      `foo/bar`, `bar`, "No extension, so the :term:`filename` is the file stem"
      `foo/bar/.`, `.`,
      `foo/bar/..`, `..`
      `foo/bar/`, `.`, The :term:`filename` is "`.`"
      `.hidden.txt`, `.hidden`,

  parent filepath

    .. default-role:: term

    The *parent* filepath of some `filepath` :math:`P` is the substring of
    :math:`P` that identifies the directory that would contain the file
    identified by :math:`P`. If :math:`P` is only a `filename`, the parent
    filepath is the single ASCII dot "``.``" to refer to the
    `working directory`.
