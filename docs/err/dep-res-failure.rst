Error: Dependency Resolution Failure
####################################

.. note::
    ``dds`` implements the `Pubgrub`_ dependency resolution algorithm.

.. _Pubgrub: https://github.com/dart-lang/pub/blob/master/doc/solver.md

If you receive this error, it indicates that the requested dependencies create
one or more conflicts. ``dds`` will do its best to emit a useful explanation of
how this conflict was formed that can hopefully be used to find the original
basis for the conflict.

Every package can have some number of dependencies, which are other packages
that are required for the dependent package to be used. Beyond just a package
name, a dependency will also have a *compatible version range*.


Resolution Example
******************

For example, let us suppose a package ``Widgets@4.2.8`` may have a *dependency*
on ``Gadgets^2.4.4`` and ``Gizmos^3.2.0``.

.. note::
    The ``@4.2.8`` suffix on ``Widgets`` means that ``4.2.8`` is the *exact*
    version of ``Widgets``, while the ``^2.4.4`` is a *version range* on
    ``Gadgets`` which starts at ``2.4.4`` and includes every version until (but
    not including) ``3.0.0``. ``^3.2.0`` is a version range on ``Gizmos`` that
    starts at ``3.2.0`` and includes every version until (but not including)
    ``4.0.0``.

Now let us suppose the following versions of ``Gadgets`` and ``Gizmos`` are
available:

``Gadgets``:
    - ``2.4.0``
    - ``2.4.3``
    - ``2.4.4``
    - ``2.5.0``
    - ``2.6.0``
    - ``3.1.0``

``Gizmos``:
    - ``2.1.0``
    - ``3.2.0``
    - ``3.5.6``
    - ``4.5.0``

We can immediately rule out some candidates of ``Gadgets``: for the dependency
``Gadgets^2.4.4``, ``2.4.0`` and ``2.4.3`` are *too old*, while ``3.1.0`` is
*too new*. This leaves us with ``2.4.4``, ``2.5.0``, and ``2.6.0``.

We'll first look at ``Gadgets@2.4.4``. We need to recursively solve its
dependencies. Suppose that it declares a dependency of ``Gizmos^2.1.0``. We
have already established that we *require* ``Gizmos^3.2.0``, and because
``^2.1.0`` and ``^3.2.0`` are *disjoint* (they share no common versions) we can
say that ``Gizmos^3.2.0`` is *incompatible* with our existing partial solution,
and that its dependent, ``Gadgets@2.4.4`` is *transitively incompatible* with
the partial solution. Thus, ``Gadgets@2.4.4`` is out of the running.

This doesn't mean we're immediately broken, though. We still have two more
versions of ``Gadgets`` to inspect. We'll start with the next version in line:
``Gadgets@2.5.0``. Suppose that it has a dependency on ``Gizmos^3.4.0``. We
have already established a requirement of ``Gizmos^3.2.0``, so we must find
a candidate for ``Gizmos`` that satisfies both dependencies. Fortunately, we
have exactly one: ``Gizmos@3.5.6`` satisfies *both* ``Gizmos^3.2.0`` *and*
``Gizmos^3.4.0``.

Suppose that ``Gizmos@3.5.6`` has no further dependencies. At this point, we
have inspected all dependencies and have resolutions for every named package:
Thus, we have a valid solution of ``Widgets@4.2.8``, ``Gadgets@2.5.0``, and
``Gizmos@2.6.0``! We didn't even need to inspect ``Gadgets@2.6.0``.

In this case, ``dds`` will not produce an error, and the given package solution
will be used.


Breaking the Solution
=====================

Now suppose the same case, except that ``Gadgets@2.5.0`` is not available.
We'll instead move to check ``Gadgets@2.6.0``.

Suppose that ``Gadgets@2.6.0`` has a dependency on ``Gizmos^4.0.6``. While we
*do* have a candidate thereof, we've already declared a requriement on
``Gizmos^3.2.0``. Because ``^4.0.6`` and ``^3.2.0`` are disjoint, then there is
no possible satisfier for both ranges. This means that ``Gizmos^4.0.6`` is
incompatible in the partial solution, and that ``Gadgets@2.6.0`` is
transitively incompatible as well. It is no longer a candidate.

We've exhausted the available candidates for ``Gadgets^2.4.4``, so we must now
conclude that ``Gadgets^2.4.4`` is *also incompatible*. Transitively, this also
means that ``Widgets@4.2.8`` is incompatible as well.

We've reached a problem, though: ``Widgets@4.2.8`` is our original requirement!
There is nothing left to invalidate in our partial solution, so we rule that
our original requirements are *unsatisfiable*.

At this point, ``dds`` will raise the error that *dependency resolution has
failed*. It will attempt its best to reconstruct the logic that we have used
above in order to explain what has gone wrong.


Fixing the Problem
******************

There is no strict process for fixing these conflicts.

Fixing a dependency conflict is a manual process. It will require reviewing the
available versions and underlying reasons that the dependency maintainers have
chosen their compatibility ranges statements.

Your own dependency statements will often need to be changed, and sometimes
even code will have to be revised to reach compatibility with newer or older
dependency versions.
