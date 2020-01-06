Error: Unknown Usage/Linking Requirements
#########################################

A library can declare that it *uses* or *links* to another library by using the
``Uses`` and ``Links`` keys in ``library.dds``, respectively.

These requirements are specified by using the ``Namespace/Name`` pair that
identifies a library. These are defined by both the project's dependencies and
the project itself. If a ``Uses`` or ``Links`` key does not correspond to a
known library, ``dds`` will not be able to resolve the usage requirements, and
will generate an error.

To fix this issue, ensure that you have correctly spelled the library
identifier and that the package that contains the respective library has been
declared as a dependency of the library that is trying to use it.
