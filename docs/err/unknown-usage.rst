Error: Unknown Usage/Linking Requirements
#########################################

A library can declare that it *uses* another library by using the
``using`` keyword in its ``pkg.yaml`` or ``pkg.json`` file.

These requirements are specified by using the ``name`` that identifies the
library. If the ``using`` is not within a dependency declaration, then the
``using`` must name another library within the same package as the library that
contains the ``using`` library. If the ``using`` keyword appears within a
dependency declaration, that ``using`` must specify a library that belongs to
the package that is identified by the dependency. If a ``using`` does not
correspond to a known library, ``dds`` will not be able to resolve the usage
requirements, and will generate an error.

To fix this issue, ensure that you have correctly spelled the library identifier
and that the package contains the respective library.
