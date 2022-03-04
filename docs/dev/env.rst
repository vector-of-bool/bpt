Setting Up a Build/Development Environment
##########################################

While ``dds`` is able to build itself, several aspects of build infrastructure
are controlled via Python scripts. You will need Python 3.7 or later available
on your system to get started.


.. _Poetry: https://python-poetry.org

Getting Started with *Poetry*
*****************************

``dds`` CI runs atop `Poetry`_, a Python project management tool. While designed
for Python projects, it serves our purposes well.


Installing Poetry
=================

If you do not have Poetry already installed, it can be obtained easily for most
any platform.
`Refer to the Poetry "Installation" documentation to learn how to get Poetry on your platform <https://python-poetry.org/docs/#installation>`_.

The remainder of this documentation will assume you are able to execute
``poetry`` on your command-line.


Setting Up the Environment
==========================

To set up the scripts and Python dependencies required for CI and development,
simply execute the following command from within the root directory of the
project::

  $ poetry install

Poetry will then create a Python virtual environment that contains the Python
scripts and tools required for building and developing ``dds``.

The Python virtual environment that Poetry created can be inspected using
``poetry env info``, and can be deleted from the system using
``poetry env remove``. Refer to
`the Poetry documentation <https://python-poetry.org/docs>`_ for more
information about using Poetry.


Using the Poetry Environment
****************************

Once the ``poetry install`` command has been executed, you will now be ready to
run the ``dds`` CI scripts and tools.

The scripts are installed into the virtual environment, and should not be
globally installed anywhere else on the system. You can only access these
scripts by going through Poetry. To run any individual command within the
virtual environment, use ``poetry run``::

  $ poetry run <some-command>

This will load the virtual environment, execute ``<some-command>``, then exit
the environment. This is useful for running CI scripts from outside of the
virtualenv.

**Alternatively**, the environment can be loaded persistently into a shell
session by using ``poetry shell``::

  $ poetry shell

This will spawn a new interactive shell process with the virtual environment
loaded, and you can now run any CI or development script without needing to
prefix them with ``poetry run``.

Going forward, the documentation will assume you have the environment loaded
as-if by ``poetry shell``, but any ``dds``-CI-specific command can also be
executed by prefixing the command with ``poetry run``.


Working With an MSVC Environment in VSCode
==========================================

If you use Visual Studio Code as your editor and MSVC as your C++ toolchain,
you'll need to load the MSVC environment as part of your build task. ``dds`` CI
has a script designed for this purpose. To use it, first load up a shell within
the Visual C++ environment, then, from within the previously create Poetry
environment, run ``gen-msvs-vsc-task``. This program will emit a Visual Studio
Code JSON build task that builds ``dds`` and also contains the environment
variables required for the MSVC toolchain to compile and link programs. You can
save this JSON task into ``.vscode/tasks.json`` to use as your primary build
task while hacking on ``dds``.
