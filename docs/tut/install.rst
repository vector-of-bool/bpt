Getting/Installing ``dds``
##########################

``dds`` ships as a single statically linked executable. It does not have any
installer or distribution package.


Downloading
***********

Downloads are available on `the main dds website <https://dds.pizza/downloads>`_
as well as
`the GitHub Releases page <https://github.com/vector-of-bool/dds/releases>`_.
Select the executable appropriate for your platform.

Alternatively, the appropriate executable can be downloaded directly from the
command-line with an easy-to-remember URL. Using ``curl``:

.. code-block:: bash

  # For Linux, writes a file in the working directory called "dds"
  curl dds.pizza/get/linux -Lo dds

  # For macOS, writes a file in the working directory called "dds"
  curl dds.pizza/get/macos -Lo dds

Or using PowerShell:

.. code-block:: powershell

  # Writes a file in the working directory called "dds.exe"
  Invoke-WebRequest dds.pizza/get/windows -OutFile dds.exe

**On Linux, macOS, or other Unix-like systems**, you may need to mark the
downloaded file as executable:

.. code-block:: bash

  # Add the executable bit to the file mode for the file named "dds"
  chmod +x dds


Installing
**********

Note that it is not necessary to "install" ``dds`` before it can be used.
``dds`` is a single standalone executable that can be executed in whatever
directory it is placed. If you are running a CI process and need ``dds``, it is
viable to simply download the executable and place it in your source tree and
execute it from that directory.

**However:** If you want to be able to execute ``dds`` with an unqualified
command name from any shell interpreter, you will need to place ``dds`` on a
directory on your shell's ``PATH`` environment variable.


Easy Mode: ``install-yourself``
===============================

``dds`` includes a subcommand "``install-yourself``" that will move its own
executable to a predetermined directory and ensure that it exists on your
``PATH`` environment variable. It is simple enough to run the command::

  $ ./dds install-yourself

This will copy the executable ``./dds`` into a user-local directory designated
for containing user-local executable binaries. On Unix-like systems, this is
``~/.local/bin``, and on Windows this is ``%LocalAppData%/bin``. ``dds`` will
also ensure that the destination directory is available on the ``PATH``
environment variable for your user profile.

.. note::

  If ``dds`` reports that is has modified your PATH, you will need to restart
  your command line and any other applications that wish to see ``dds`` on your
  ``PATH``.

.. note::

  The ``install-yourself`` command accepts some other options. Pass ``--help``
  for more information.


Manually: On Unix-like Systems
==============================

For an **unprivileged, user-specific installation (preferred)**, we recommend
placing ``dds`` in ``~/.local/bin`` (Where ``~`` represents the ``$HOME``
directory of the current user).

Although not officially standardized,
`the XDG Base Directory specification <https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html>`_
recommends several related directories to live within ``~/.local`` (and ``dds``
itself follows those recommendations for the most part).
`The systemd file heirarchy <https://www.freedesktop.org/software/systemd/man/file-hierarchy.html>`_
also recommends placing user-local binaries in ``~/.local/bin``, and several
Linux distribution's shell packages add ``~/.local/bin`` to the startup
``$PATH``.

Placing a file in ``~/.local/bin`` requires no privileges beyond what the
current user can execute, and gives a good isolation to other users on the
system. Other tools (e.g. ``pip``) will also use ``~/.local/bin`` for the
installation of user-local scripts and commands.

.. note::

  On some shells, ``~/.local/bin`` is not an entry on ``$PATH`` by default.
  Check if your shell's default ``$PATH`` environment variable contains
  ``.local/bin``. If it does not, refer to your shell's documentation on how to
  add this directory to the startup ``$PATH``.

For a **system-wide installation**, place the downloaded ``dds`` executable
within the ``/usr/local/bin/`` directory. This will be a directory on the
``PATH`` for any Unix-like system.

.. warning::

  **DO NOT** place ``dds`` in ``/usr/bin`` or ``/bin``: These are reserved for
  your system's package management utilities.


Manually: On Windows
====================

Unlike Unix-like systems, Windows does not have a directory designated for
user-installed binaries that lives on the ``PATH``. If you have a directory that
you use for custom binaries, simply place ``dds.exe`` in that directory.

If you are unfamiliar with placing binaries and modifying your ``PATH``, read
on:

For an **unprivileged, user-specific installation**, ``dds`` should be placed in
a user-local directory, and that directory should be added to the user ``PATH``.

To emulate what ``dds install-yourself`` does, follow the following steps:

#. Create a directory ``%LocalAppData%\bin\`` if it does not exist.

   For ``cmd.exe``

   .. code-block:: batch

      md %LocalAppData%\bin

   Or for PowerShell:

   .. code-block:: powershell

      md $env:LocalAppData\bin

#. Copy ``dds.exe`` into the ``%LocalAppData%\bin`` directory.
#. Go to the Start Menu, and run "Edit environment variables for your account"
#. In the upper area, find and open the entry for the "Path" variable.
#. Add an entry in "Path" for ``%LocalAppData%\bin``.
#. Confirm your edits.
#. Restart any applications that require the modified environment, including
   command-lines.

If the above steps are performed successfully, you should be able to open a new
command window and execute ``dds --help`` to get the help output.
