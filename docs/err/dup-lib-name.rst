Error: Duplicate Library Identifier
###################################

Libraries in ``dds`` are represented by a *namespace* and a *name*, joined
together with a forward-slash "``/``". Suppose that we have a library named
``Gadgets`` that lives in the ``ACME`` library-namespace. The combined library
identifier would be ``ACME/Gadgets``.

.. note::
    The "namespace" of a library in this case is arbitrary and not necessarily
    associated with any C++ ``namespace``.

If more than one library declares itself to have the same ``name`` and lives in
the same ``namespace``, ``dds`` will issue an error.

To avoid this error in your own project and to avoid causing this error in your
downstream consumers, the ``namespace`` of your package should be considered
carefully and be unique. Do not use a ``namespace`` that is likely to be used
by another developer or organization, especially generic names.

If you are seeing this issue and it names a library that you do not own, it
means that two or more of your dependencies are attempting to declare a library
of the same ``name`` in the same ``namespace``. This issue should be raised
with the maintainers of the packages in question.

.. seealso::
    For more information, refer to the :ref:`pkgs.pkgs` section and the
    :ref:`pkgs.libs` section.