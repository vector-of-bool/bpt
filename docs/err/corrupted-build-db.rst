Error: The build database is corrupted
######################################

``dds`` stores various data about the local project's build in a database file
kept in the build output directory (default is ``_build``). The file is named
``.dds.db``, and can be safely deleted with no ill effects on the project.

This error is not caused by regular user action, and is probably a bug in
``dds``. If you see this error message frequently, please file a bug report.