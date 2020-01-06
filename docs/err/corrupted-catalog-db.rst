Error: The catalog database appears corrupted/invalid
#####################################################

This issue will occur if the schema contained in the catalog database does not
match the schema that ``dds`` expects. This should never occur during normal
use of ``dds``.

.. note::
    If you suspect that ``dds`` has accidentally corrupted its own catalog
    database, please file a bug report.

``dds`` cannot reliably repair a corrupted database, and the only solution will
be to either manually fix it or to delete the database file and start fresh.

.. note::
    Deleting the catalog database will lose any customizations that were
    contained in that catalog, and they will need to be reconstructed.
