Error: No such package in the catalog
#####################################

This error will occur when one attempts to obtain a package from the package
catalog by its specific ``name@version`` pair, but no such entry exists
in the catalog.

It is possible that the intended package *does exist* but that the spelling of
the package name or version number is incorrect. Firstly, check your spelling
and that the version number you have requested is correct.

In another case, it is possible that the package *exists somewhere*, but has
not been loaded into the local catalog. As of this writing, ``dds`` does not
automatically maintain the catalog against a central package repository, so
package entries must be loaded and imported manually. If you believe this to be
the case, refer to the section on the :doc:`/guide/catalog`, especially the
section :ref:`catalog.adding`.
