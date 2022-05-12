#####################
Dependencies in |bpt|
#####################

.. default-role:: term
.. highlight:: yaml

In |bpt| a `project` or `library` can declare dependencies on
`libraries <library>` in external `packages <package>`. A dependency consists of
an external package `name`, a range of acceptable :ref:`versions <semver>`, and
some set of libraries to *use* from that external package.

Dependencies can be set using the :yaml:`dependencies` property on either the
top-level of |bpt.yaml|, or as a property attached to an invidual library within
the project. The similar :yaml:`test-dependencies` key specifies dependencies
that are required to build and execute the project's `tests <test>`.

These dependency properties should be an array of
*dependency specifiers*.

.. _dep-spec:

Dependency Specifiers
#####################

The :yaml:`dependencies` and :yaml:`test-dependencies` arrays in |bpt.yaml| are
used to declare `dependencies <dependency>` on libraries in external packages::

  dependencies:
    - dep-name@1.2.3

The above specifier requests a package named "``dep-name``" with a version range
of greater-or-equal-to-``1.2.3`` and less-than ``2.0.0``, and uses the
`default library` from that project (The library with the same name as the
package itself).

If a package provides more than one library, or does not use a
`default library`, the dependency specifier can include a :cpp:`using` clause to
name one or more comma-separated library names to use from the package::

  dependencies:
    - acme@1.2.3 using widgets, gadgets

If the :cpp:`using` clause is omitted an implicit ":cpp:`using [pkg-name]`" is
assumed, with ``[pkg-name]`` being the name of the depended-on package.

More formally: A dependency specifier (:token:`dep-spec`) is a string of the
following format:

.. productionlist::
  dep-spec: `dep_name` `dep_range` ["using" `lib_name` ("," `lib_name`)+]
  dep_name: name
  lib_name: name
  dep_range: `range_sym` dep_version
  range_sym: "@" | "^" | "~" | "=" | "+"
  dep_version: version

alternatively::

  <dep_name>{@,^,~,=,+}<version> [using <lib_name> [, <lib_name> [...]]]

- The :token:`dep_name` must be the `name` of an external package.
- The :token:`dep_version` must be a valid
  :ref:`Semantic Version number <semver>`.
- The choice of the :token:`range_sym` symbol alters the semantics of the
  version range. Refer: :ref:`deps.ranges`
- If :cpp:`using` is provided, the :token:`lib_name` tokens must be the names of
  one or more libraries within the external package that are to be used. The
  :cpp:`using` suffix may be omitted.


.. _deps.ranges:

Compatible Range Specifiers
***************************

When specifying a dependency on a package, one will want to specify which
versions of the dependency are supported.

.. note::

    Unlike other packaging tools, |bpt| will find a solution with the *lowest*
    possible version that satisfies the given requirements for each package.
    This decision is not incidental: It's entirely intentional. Refer to:
    :ref:`deps.ranges.why-lowest`.

|bpt| compatible-version ranges use similar syntax to other tools. There are
four version range kinds available, listed in order of most-to-least
restrictive:

Exact: ``=1.2.3``
  Specifies an *exact* requirement. The dependency must match the named
  version *exactly* or it is considered incompatible.

Minor: ``~1.2.3``
  Specifies a *minor* requirement. The version of the dependency should be
  *at least* the given version, but not as new or newer than the next minor
  revision. In this example, it represents the half-open version range
  ``[1.2.3, 1.3.0)``.

Major: ``^1.2.3`` or ``@1.2.3``
  Specifies a *major* requirement. The version must be *at least* the same
  given version, but not any newer than the the next major version. In the
  example, this is the half-open range ``[1.2.3, 2.0.0)``.

  .. note::
    This is the recommended default option to reach for, as it matches the
    intended behavior of `Semantic Versioning <https://semver.org>`_.

At-least: ``+1.2.3``
  Specifies an *at least* requirement. The version must be *at least* the
  given version, but any newer version is acceptable.


.. _deps.ranges.why-lowest:

Why Pull the *Lowest* Matching Version?
#######################################

When resolving dependencies, |bpt| will pull the version of the dependency
that is the lowest version that satisfies the given range. In most cases,
this will be the same version that is the base of the version range.

Imagine a scenario where we *did* select the "latest-matching-version":

Suppose we are developing a library ``Gadgets``, and we wish to make use of
``Widgets``. The latest version is ``1.5.2``, and they promise Semantic
Versioning compatibility, so we select a dependency statement of
``Widgets^1.5.2``.

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
   in ``1.6.0``, and thus makes the dependency declaration on ``Widgets^1.5.2``
   a *lie*.

Pulling the lowest-matching-version has two *huge* benefits:

#. No automatic CI upgrades. The code built today will produce the same result
   when built a year from now.
#. Using a feature/fix beyond our minimum requirement becomes a compile error,
   and we catch these up-front rather than waiting for a downstream user
   discovering them for us.


.. rubric:: *Isn't this what lockfiles are for?*

Somewhat. Lockfiles will prevent automatic upgrades, but they will do nothing
to stop accidental reliance on new versions. There are other useful features
of lockfiles, but preventing automatic upgrades can be a non-issue by simply
using lowest-matching-version.


.. rubric:: *So, if this is the case, why use ranges at all?*

In short: *Your* compatibility ranges are not for *you*. They are for *your
users*.

Suppose package ``A`` requires ``B^1.0.0``, and ``B`` requires ``C^1.2.0``.
Now let us suppose that ``A`` wishes to use a newer feature of ``C``, and thus
declares a dependency on ``C^1.3.0``. ``B`` and ``A`` have different
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


Dependency Compatibility and the :cpp:`using` Specifier
#######################################################

Besides requiring that a candidate for dependency resolution meet the version
requirements, the candidate must also provide all of the libraries named by the
:cpp:`using` specifier on the dependency statement.

.. default-role:: math

A Simple Example
****************

For example, suppose that the following packages are available:

- ``acme-libs@1.2.0`` - Provides one library: ``widgets``.
- ``acme-libs@1.3.0`` - Provides libraries ``widgets`` and ``gadgets``.
- ``acme-libs@1.4.0`` - Provides libraries ``gadgets``, and
  ``gizmos``

Suppose now I have a project |bpt.yaml|:

.. code-block:: yaml

  name: my-code
  version: 4.2.4

  dependencies:
    - acme-libs@1.0.0 using gadgets, widgets

Our package contains a single dependency statement `R_1` of
``acme-libs@1.0.0 using gadgets, widgets``. When |bpt| does dependency
resolution, it sees the requirement on ``acme-libs`` and seeks out a compatilbe
version. Since |bpt| prefers to find the *lowest-matching-version*, it begins by
considering ``acme-libs@1.2.0``. Good news: This matches the *version*
requirement of `R_1`! Bad news: Our dependency `R_1` has a
:cpp:`using gadgets, widgets`, and the ``1.2.0`` version of ``acme-libs`` does
not provide the required ``gadgets`` library.

|bpt| will mark ``acme-libs@1.2.0`` as *incompatible* with `R_1` and move on to
the next candidate: ``acme-libs@1.3.0``. Great news: This version both matches
the version requirement `R_1` *and* provides *both* libraries required by `R_1`.
Thus, ``acme-libs@1.3.0`` will be selected to solve `R_1`.

Since `R_1` is the only dependency statement, we have a complete dependency
solution with just selecting ``acme-libs@1.3.0``.


Getting More Complicated
************************

Suppose now that there are additional packages available for use:

- ``gandalf@6.3.0``

  - Provides library ``wizard`` which depends on
    ``acme-libs@1.2.0 using gizmos``.

- ``gandalf@6.4.0``

  - Provides ``wizard``, which depends on ``acme-libs@1.2.0 using gadgets``

Let's update our |bpt.yaml| to use this package:

.. code-block:: yaml

  name: my-code
  version: 4.3.0

  dependencies:
    - acme-libs@1.0.0 using gadgets, widgets
    - gandalf@6.0.0 using wizard

In addition to our previous dependency `R_1` of
``acme-libs@1.0.0 using gadgets, widgets``, we now have an additional
requirement `R_2` of ``gadgalf@6.0.0 using wizard``. Dependency resolution now
becomes more complex:

1. In solving `R_2`, we first check ``gandalf@6.3.0``

   1. This looks okay at first: This package matches our version requirement in
      `R_2` *and* it also provides the ``wizard`` library that we are
      :cpp:`using` in `R_2`.

   2. |bpt| will speculatively select this package as part of the solution.

2. |bpt| will now validate the new package against the "partial solution" that
   we are working with.

3. The used ``wizard`` library of the selected ``gandalf@6.3.0`` has its own
   dependency `R_g1` of ``acme-libs@1.2.0 using gizmos``. We can take the
   *intersection* `R_x = R_1 \cap R_g1` of our existing requirement `R_1` on
   ``acme-libs`` to form a new *derived* requirement `R_x =`
   ``acme-libs@1.0.0 using gadgets, widgets, gizmos``

   This `R_x` is the dependency *intersection* of `R_g1` and `R_1` because any
   selection that satisfies `R_x` will necessarily also satisfy `R_g1` and
   `R_1`.

4. |bpt| must now seek a version of ``acme-libs`` that satisfies `R_x`, but a
   cursory glance reveals that `R_x` is *unsatisfiable*: There is no
   ``acme-libs`` package that provides ``gadgets`` *and* ``widgets`` *and*
   ``gizmos``. (|bpt| encodes this fact with a special requirement
   ``acme-libs@[⊥]``, which is unsatisfiable *by definition*. This notation may
   appear in diagnostics during dependency resolution failure.)

5. Because our partial solution contains an unsatisfiable derived requirement,
   the entire partial solution is invalid, and |bpt| must backtrack to find the
   speculative decision that caused the failure. In this case, the speculative
   selection of ``gandalf@6.3.0`` caused the creation of an unsatisfiable
   partial solution, so we transitively mark ``gandalf@6.3.0`` as *incompatible*
   with the partial solution that led to its selection.

6. With ``gandalf@6.3.0`` ruled out, we need to find another package to satisfy
   `R_2`. Fortunately, we have one: ``gandalf@6.4.0``. This is speculatively
   selected for the solution.

7. The ``gandalf@6.4.0`` library ``wizard`` contains a new requirement `R_g2` of
   ``acme-libs@1.2.0 using gadgets``, which we will check against our existing
   partial solution.

8. The speculated selection of ``acme-libs@1.3.0`` satisfies ``R_g2``, so the
   partial solution is okay.

9. There are no remaining unsatisfied requirements, so we select
   ``acme-libs@1.3.0`` and ``gandalf@6.4.0`` as our dependency solution.


A Note on Library *Removal*
***************************

In the above example, the ``acme-libs@1.4.0`` simltaneously *added* the
``gizmos`` library and *removed* the ``widgets`` library.

The removal of a library from a package is necessarily a breaking change, and is
especially troublesome here in that there is no version of the package that
contains all of ``widgets``, ``gadgets``, and ``gizmos``. Any dependency
statement (derived or direct) that requests all three libraries will be
unsatisfiable. It is very highly recommended to refrain from removing libraries
as part of a minor version change, thus reducing the likelihood of such
conflicts.

