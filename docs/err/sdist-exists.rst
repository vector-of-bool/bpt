Error: Source Distribution Already Exists
#########################################

This error is presented when an attempt is made to export/create a source
distribution of a package in a way that would overwrite an existing source
distribution.

**If exporting to a repository**, this means that a source distribution with
the same name and version is already present in the repository. The
``--replace`` option can be used to make ``dds`` forcibly overwrite the source
distribution in the repository. This will be a common workflow when developing
a package and one desires to see those changes reflected in another project
that is try to use it.

**If creating a source distribution manually**, this means that the destination
path of the source distribution directory is already an existing directory
(which may not be a source distribution itself). If ``dds`` were to try and
write a source distribution to the named path, it would be required to delete
whatever exists there before creating the source distribution.

.. warning::
    When using ``dds sdist create`` with the ``--out <path>`` parameter, the
    ``<path>`` given **is not the directory in which to place the source
    distribution, but the filepath to the source distribution itself**!

    If I have a directory named ``foo/``, and I want to create a source
    distribution in that directory, **the following command is incorrect**::

        # Do not do this:
        dds sdist create --out foo/

    If you pass ``--replace`` to the above command, ``dds`` will **destroy the
    existing directory** and replace it with the source distribution!

    You **must** provide the full path to the source distribution::

        # Do this:
        dds sdist create --out foo/my-project.dsd
