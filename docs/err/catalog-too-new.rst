Error: The package catalog database is for a later ``dds`` version
##################################################################

If you receive this error, it indicates that the schema version of a catalog
database is newer than the schema expected by the ``dds`` process that is
trying to actually make use of the catalog. ``dds`` catalog databases are not
forwards-compatible and cannot be downgraded.

If you generated/modified the catalog using a ``dds`` executable that is newer
than the one you attempted to open it with, it is possible that the newer
``dds`` executable performed a schema upgrade that is unsupported by the older
version of ``dds``.

If you have not specified a path to a catalog, ``dds`` will use the user-local
default catalog database. If you receive this error while using the user-local
database, the solution is to either upgrade ``dds`` to match the newer catalog
or to delete the user-local catalog database file.

.. note::
    Deleting the catalog database will lose any customizations that were
    contained in that catalog, and they will need to be reconstructed.
