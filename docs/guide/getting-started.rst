Getting Started
###############

Using ``dds`` is extremely simple:

#. Create a new directory. This will be our *package root*.
#. Create a subdirectory named ``src/``.
#. Create a file named ``application.main.cpp``.
#. Add a ``main()`` function to our new source file.
#. Depending on your compiler:
    #. If you are using GCC, run the ``dds build -t :gcc`` command from the
       package root.
    #. If you are using Clang, run the ``dds build -t :clang`` command from the
       package root.
    #. If you are using Visual C++, run the :code:`dds build -t :msvc` command
       from the package root from a developer command prompt.

You will now have a ``_build`` directory, and it will contain a newly compiled
``application``!

Obviously this isn't *all* there is to do with ``dds``. Read on to the next
pages to learn more.

.. note::
    You're reading a very early version of these docs. There will be a lot more
    here in the future. Watch this space for changes!
