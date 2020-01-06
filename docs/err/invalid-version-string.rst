Error: Invalid version string
#############################

``dds`` stores version numbers in a type-safe manner, and all version numbers
are requried to match `Semantic Versioning <https://semver.org>`_.

If you see this error, it means that a ``dds`` found a version number that does
not correctly conform to Semantic Versioning's requirements for version numbers
refer to the ``dds`` output for a description of *where* the bad version string
was found, and refer to the Semantic Versioning website for information about
how to properly format a version number.


.. _range:

Version Ranges
**************

In addition to requirements on individual versions, providing a version range
requires a slightly different syntax.

.. seealso:: :ref:`deps.ranges`
