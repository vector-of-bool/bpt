Error: The generated source distribution's identity is not correct
##################################################################

When ``dds`` attempts to automatically generate a source distribution,
especially when generating from a remote that was acquired using a catalog
listing, ``dds`` expects the generated source distribution to have a matching
package identity to match what was intended.

This can happen if a catalog listing's package version does not match the
source of the remote acquisition method. For example, if using ``git`` to clone
a repository, the ``git-ref`` used to clone must match the package version of
the listing. If the ``git-ref`` is a branch, it is possible that additional
changes were pushed into the branch that changed the package version, thus
invalidating the package. [#f1]_

.. [#f1]
    For this reason, it is **highly recommended** to use Git *tags* to
    refer to remote packages *instead of branches*.
