``bpt install-yourself``
########################

``bpt install-yourself`` is a utility subcommand used to install |bpt| on the
``PATH`` for execution from any command-line. Refer:
:ref:`tut.install.install-yourself`.

.. program:: bpt install-yourself

.. option:: --where {user,system}

    :default: ``user``

    The intended scope of the installation. For ``system``, installs in a global
    directory for all users of the system. For ``user``, installs in a
    user-specific directory for executable binaries.

    User-local installation is highly recommended.d

.. option:: --dry-run

    If specified, |bpt| will only write what *would* happen for the installation
    without performing any action.

.. option:: --no-modify-path

    If specified, |bpt| will not attempt to update any ``PATH`` environment
    variables.

.. option:: --symlink

    If specified, |bpt| will install a symoblic link to the |bpt| executable
    that was used with ``install-yourself``, instead of copying the executable
    to the destination.
