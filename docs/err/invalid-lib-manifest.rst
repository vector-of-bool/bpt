Error: Invalid library manifest
###############################

Every ``dds`` package must contain a valid library manifest for each library
that it exports. These manifests are stored in the library root to which they
correspond, and are stored in JSON5 format in ``library.json5`` (or similarly
named file).

The contents of this file must follow a prescribed content, or ``dds`` will
reject the manifest. Refer to the :ref:`pkgs.libs` documentation page for more
information about how to declare libraries.
