Error: Creating a static library archive failed
###############################################

``dds`` primarily works with *static libraries*, and the file format of a
static library is known as an *archive*. This error indicates that ``dds``
failed in its attempt to collect one or more compiled *object files* together
into an archive.

It is unlikely that regular user action can cause this error, and is more
likely an error related to the environment and/or filesystem.

If you are using a custom toolchain and have specified the archiving command
explicitly, it may also be possible that the argument list you have given to
the archiving tool is invalid.

When archiving fails, ``dds`` will print the output and command that tried to
generate the archive. It should provide more details into the nature of the
failure.
