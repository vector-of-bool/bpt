Error: Invalid built-in toolchain
#################################

``dds`` requires a toolchain in order to build any packages or projects. It
ships with several built-in toolchains that do not require the user to write
any configuration files.

If you start your toolchain name (The `-t` or `--toolchain` argument)
with a leading colon, dds will interpret it as a reference to a built-in
toolchain. (Toolchain file paths cannot begin with a leading colon).

These toolchain names are encoded into the dds executable and cannot be
modified.

.. seealso::
    Refer to the documentation on :doc:`toolchains </guide/toolchains>` and
    :ref:`toolchains.builtin`.
