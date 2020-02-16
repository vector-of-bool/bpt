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
name and version in the ``package.json5`` in the package root:

.. code-block:: js

    {
        name: 'acme-widgets',
        version: '4.3.6',
        namespace: 'acme',
    }

.. note::
    The ``namespace`` field is required, but will be addressed in the
    :ref:`deps.lib-deps` section.

Suppose that our package's libraries build upon the libraries in the
``acme-widgets`` package, and that we require version ``1.4.3`` or newer, but
not as new as ``2.0.0``. Such a dependency can be declared with the ``Depends``
key:

.. code-block:: js
    :emphasize-lines: 5-7

    {
        name: 'acme-gadgets',
        version: '4.3.6',
        namespace: 'acme',
        depends: {
            'acme-widgets': '^1.4.3',
        },
    }

.. seealso:: :ref:`deps.ranges`.

If we wish to declare additional dependencies, we simply declare them with
additional ``Depends`` keys

.. code-block::
    :emphasize-lines: 7-8

    {
        name: 'acme-gadgets',
        version: '4.3.6',
        namespace: 'acme',
        depends: {
            'acme-widgets': '^1.4.3',
            'acme-gizmos': '~5.6.5',
            'acme-utils': '^3.3.0',
        },
    }

When ``dds`` attempts to build a project, it will first build the dependency
solution by iteratively scanning the dependencies of the containing project and
all transitive dependencies.


.. _deps.ranges:

Compatible Range Specifiers
===========================

When specifying a dependency on a package, one will want to specify which
versions of the dependency are supported.

.. note::
    Unlike other packaging tools, ``dds`` will find a solution with the
    *lowest* possible version that satisfies the given requirements for each
    package. This decision is not incidental: It's entirely intentional.
    Refer to: :ref:`deps.ranges.why-lowest`.

``dds`` compatible-version ranges are similar to the shorthand range specifiers
supported by ``npm`` and ``npm``-like tools. There are five (and a half)
version range formats available, listed in order of most-to-least restrictive:

Exact: ``1.2.3`` and ``=1.2.3``
    Specifies an *exact* requirement. The dependency must match the named
    version *exactly* or it is considered incompatible.

Minor: ``~1.2.3``
    Specifies a *minor* requirement. The version of the dependency should be
    *at least* the given version, but not as new or newer than the next minor
    revision. In this example, it represents the half-open version range
    ``[1.2.3, 1.3.0)``.

Major: ``^1.2.3``
    Specifies a *major* requirement. The version must be *at least* the same
    given version, but not any newer than the the next major version. In the
    example, this is the half-open range ``[1.2.3, 2.0.0)``.

    .. note::
        This is the recommended default option to reach for, as it matches the
        intended behavior of `Semantic Versioning <https://semver.org>`_.

At-least: ``+1.2.3``
    Specifies an *at least* requirement. The version must be *at least* the
    given version, but any newer version is acceptable.

Anything: ``*``
    An asterisk ``*`` represents than *any* version is acceptable. This is not
    recommended for most dependencies.


.. _deps.ranges.why-lowest:

Why Pull the *Lowest* Matching Version?
---------------------------------------

When resolving dependencies, ``dds`` will pull the version of the dependency
that is the lowest version that satisfies the given range. In most cases,
this will be the same version that is the base of the version range.

Imagine a scenario where we *did* select the "latest-matching-version":

Suppose we are developing a library ``Gadgets``, and we wish to make use of
``Widgets``. The latest version is ``1.5.2``, and they promise Semantic
Versioning compatibility, so we select a version range of ``^1.5.2``.

Suppose a month passes, and ``Widgets@1.6.0`` is published. A few things
happen:

#. Our CI builds now switch from ``1.5.2`` to ``1.6.0`` *without any code
   changes*. Should be okay, right? I mean... it's still compatible, yeah?
#. Bugs in ``Widgets@1.6.0`` will now appear in all CI builds, and won't be
   reproducible locally unless we re-pull our dependencies and obtain the
   new version of ``Widgets``. This requires that we be conscientious enough to
   realize what is actually going on.
#. Even if ``Widgets@1.6.0`` introduces no new bugs, a developer re-pulling
   their dependencies will suddenly be developing against ``1.6.0``, and may
   not even realize it. In fact, this may continue for weeks or months until
   *everyone* is developing against ``1.6.0`` without realizing that they
   actually only require ``1.5.2`` in their dependency declarations.
#. Code in our project is written that presupposes features or bugfixes added
   in ``1.6.0``, and thus makes the dependency declaration on ``Widgets ^1.5.2``
   a *lie*.

Pulling the lowest-matching-version has two *huge* benefits:

#. No automatic CI upgrades. The code built today will produce the same result
   when built a year from now.
#. Using a feature/fix beyond our minimum requirement becomes a compile error,
   and we catch these up-front rather than waiting for a downstream user
   discovering them for us.


*Isn't this what lockfiles are for?*
""""""""""""""""""""""""""""""""""""

Somewhat. Lockfiles will prevent automatic upgrades, but they will do nothing
to stop accidental reliance on new versions. There are other useful features
of lockfiles, but preventing automatic upgrades can be a non-issue by simply
using lowest-matching-version.


*So, if this is the case, why use ranges at all?*
"""""""""""""""""""""""""""""""""""""""""""""""""

In short: *Your* compatibility ranges are not for *you*. They are for *your
users*.

Suppose package ``A`` requires ``B ^1.0.0``, and ``B`` requires ``C ^1.2.0``.
Now let us suppose that ``A`` wishes to use a newer feature of ``C``, and thus
declares a dependency on ``C ^1.3.0``. ``B`` and ``A`` have different
compatibility ranges on ``C``, but this will work perfectly fine **as long as
the compatible version ranges of A and B have some overlap**.

That final qualification is the reason we use compatibility ranges: To support
our downstream users to form dependency graphs that would otherwise form
conflicts if we required *exact* versions for everything. In the above example,
``C@1.3.0`` will be selected for the build of ``A``.

Now, if another downstream user wants to use ``A``, they will get ``C@1.3.0``.
But they discover that they actually need a bugfix in ``C``, so they place
their own requirement on ``C ^1.3.1``. Thus, they get ``C@1.3.1``, which still
satisfies the compatibility ranges of ``A`` and ``B``. Everyone gets along
just fine!


.. _deps.lib-deps:

Library Dependencies
********************

In ``dds``, library interdependencies are tracked separately from the packages
that contain them. A library must declare its intent to use another library
in the ``library.json5`` at its library root. The minimal content of a
``library.json5`` is the ``name`` key:

.. code-block:: js

    {
        name: 'gadgets'
    }

To announce that a library wishes to *use* another library, use the aptly-named
``uses`` key:

.. code-block:: js
    :emphasize-lines: 3-7

    {
        name: 'gadgets',
        uses: [
            'acme/widgets',
            'acme/gizmos',
            'acme/utils',
        ],
    }

Here is where the package's ``namespace`` key comes into play: A library's
qualified name is specified by joining the ``namespace`` of the containing
package with the ``name`` of the library within that package with a ``/``
between them.

It is the responsibility of package authors to document the ``namespace`` and
``name`` of the packages and libraries that they distribute.

.. note::
    The ``namespace`` of a package is completely arbitrary, and need not relate
    to a C++ ``namespace``.

.. note::
    The ``namespace`` need not be unique to a single package. For example, a
    single organization (Like Acme Inc.) can share a single ``namespace`` for
    many of their packages and libraries.

    However, it is essential that the ``<namespace>/<name>`` pair be
    universally unique, so choose wisely!

Once the ``uses`` key appears in the ``library.dds`` file of a library, ``dds``
will make available the headers for the library being used, and will
transitively propagate that usage requirement to users of the library.
