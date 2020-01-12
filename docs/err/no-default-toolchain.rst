Error: Unable to find a default toolchain to use for the build
##############################################################

Unlike other build systems, ``dds`` will not attempt to make a "best guess"
about the local platform and the available build toolchains. Instead, a
toolchain must be provided for every build. This can be named on the command
line with ``--toolchain`` or by placing a toolchain file in one of a few
prescribed locations.


Solution: Request a built-in toolchain
**************************************

A built-in toolchain can be specified with the ``--toolchain`` (or ``-t``)
argument. Refer to the :ref:`toolchains.builtin` section for more information.


Solution: Set up a default toolchain file for the project or for your local user
********************************************************************************

If no toolchain argument is given when a build is invoked, ``dds`` will search
a predefined set of filepaths for the existence of a toolchain file to use as
the "default." Writing a simple toolchain to one of these paths will cause
``dds`` to use this toolchain when none was provided.


.. seealso::
    For more information, refer to the full documentation page:
    :doc:`/guide/toolchains`
