Error: Invalid package manifest
###############################

Every ``bpt`` package must contain a valid package manifest, which is stored in
JSON5 format in ``package.json5`` (or similarly named file).

The contents of this file must follow a prescribed content, or ``bpt`` will
reject the manifest. Refer to the :ref:`pkgs.pkgs` documentation page for more
information about how to declare packages.
