.. highlight:: yaml

Library and Package Dependencies
################################

``dds`` considers that all libraries belong to a single *package*, but a single
package may contain one or more *libraries*. For this reason, and to better
interoperate with other build and packaging tools, we consider the issues of
package dependencies and library dependencies separately.


.. _deps.pkg-deps:

Package Dependencies
********************

Consider that we are creating a package ``acme-gadgets@4.3.6``. We declare the
name and version in the ``package.dds`` in the package root:

.. code-block::

    Name: acme-gadgets
    Version: 4.3.6
    Namespace: acme

.. note::
    The ``Namespace`` field is required, but will be addressed in the
    :ref:`deps.lib-deps` section.

Suppose that our package's libraries build upon the libraries in the
``acme-widgets`` package, and that we require version ``1.4.3`` or newer, but
not as new as ``2.0.0``. Such a dependency can be declared with the ``Depends``
key:

.. code-block::
    :emphasize-lines: 5

    Name: acme-gadgets
    Version: 4.3.6
    Namespace: acme

    Depends: acme-widgets ^1.4.3

If we wish to declare additional dependencies, we simply declare them with
additional ``Depends`` keys

.. code-block::
    :emphasize-lines: 5-7

    Name: acme-gadgets
    Version: 4.3.6
    Namespace: acme

    Depends: acme-widgets ^1.4.3
    Depends: acme-gizmos ~5.6.5
    Depends: acme-utils ^3.3.0

When ``dds`` attempts to build a project, it will first build the dependency
solution by iteratively scanning the dependencies of the containing project and
all transitive dependencies.

.. note::
    Unlike other packaging tools, ``dds`` will find a solution with the
    *lowest* possible version that satisfies the given requirements for each
    package.


.. _deps.lib-deps:

Library Dependencies
********************

In ``dds``, library interdependencies are tracked separately from the packages
that contain them. A library must declare its intent to use another library
in the ``library.dds`` at its library root. The minimal content of a
``library.dds`` is the ``Name`` key:

.. code-block::

    Name: gadgets

To announce that a library wishes to *use* another library, use the aptly-named
``Uses`` key:

.. code-block::
    :emphasize-lines: 3-5

    Name: gadgets

    Uses: acme/widgets
    Uses: acme/gizmos
    Uses: acme/utils

Here is where the package's ``Namespace`` key comes into play: A library's
qualified name is specified by joining the ``Namespace`` of the containing
package with the ``Name`` of the library within that package with a ``/``
between them.

It is the responsibility of package authors to document the ``Namespace`` and
``Name`` of the packages and libraries that they distribute.

.. note::
    The ``Namespace`` of a package is completely arbitrary, and need not relate
    to a C++ ``namespace``.

.. note::
    The ``Namespace`` need not be unique to a single package. For example, a
    single organization (Like Acme Inc.) can share a single ``Namespace`` for
    many of their packages and libraries.

    However, it is essential that the ``<Namespace>/<Name>`` pair be
    universally unique, so choose wisely!

Once the ``Uses`` key appears in the ``library.dds`` file of a library, ``dds``
will make available the headers for the library being used, and will
transitively propagate that usage requirement to users of the library.
