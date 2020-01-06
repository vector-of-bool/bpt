A *Hello, world!* Application
#############################

Creating a *hello world* application is very simple.


Creating a *Package Root*
*************************

To start, create a new directory for your project. This will be known as the
*package root*, and the entirety of our project will be placed in this
directory. The name and location of this directory is not important, but the
contents therein will be significant.

.. note::
   The term *package root* is further described in the :doc:`/guide/packages` page.

From here on, this created directory will simply be noted as ``<root>``. In
the examples, this will refer to the directory package root directory we have
created.


Creating the First *Source Root*
********************************

Within the package root, we create our first *source root*. Since we are
intending to compile files, we need to use the name that ``dds`` has designated
to be the source root that may contain compilable source files: ``src/``:

.. code-block:: bash

   mkdir src

You should now have a single item in the package root, at ``<root>/src/``. This
is the directory from which ``dds`` will search for source files.


Creating an Application Entrypoint
**********************************

To add a source file to our project, we simply create a file within a source
root with the appropriate file extension. Our source root is ``<root>/src/``,
so we'll place a source file in there. In addition, because we want to create
an *application* we need to designate that the source file provides an
application *entry point*, i.e. a ``main()`` function. To do this, we simply
prepend ``.main`` to the file extension. Create a file::

> <root>/src/hello-world.main.cpp

and open it in your editor of choice. We'll add the classic C++ *hello, world*
program:

.. code-block:: c++
    :linenos:
    :caption: ``<root>/src/hello-world.main.cpp``

    #include <iostream>

    int main() {
      std::cout << "Hello, world!\n";
    }


Building *Hello, World*
***********************

Now comes the fun part. It is time to actually compile the application!

.. important::
    If you intend to compile with Visual C++, the build must be executed
    from within a Visual Studio or Visual C++ development command prompt. These
    program shortcuts should be made available with any standard installation
    of the Visual C++ toolchain.

    ``dds`` **will not** automatically load the Visual C++ environment.

To build the program, we must provide ``dds`` with information about our
program toolchain. ``dds`` comes with a few "built in" toolchain options that
can be used out-of-the-box, and they'll be suitable for our purposes.

- If you are compiling with GCC, the toolchain name is ``:gcc``
- If you are compiling with Clang, the toolchain name is ``:clang``
- If you are compiling with Visual C++, the toolchain name is ``:msvc``

.. note::
    The leading colon ``:`` is important: This tells ``dds`` to use its
    built-in toolchain information rather than looking for a toolchain file of
    that name.

To execute the build, run the ``dds build`` command as in the following
example, providing the appropriate toolchain name in place of ``<toolchain>``::

> dds build -t <toolchain>

For example, if you are using ``gcc``, you would run the command as::

> dds build -t :gcc

If all successful, ``dds`` will emit information about the compile and link
process, and then exit without error.

By default, build results will be placed in a subdirectory of the package root
named ``_build``. Within this directory, you will find the generated executable
named ``hello-world`` (with a ``.exe`` suffix if on Windows).

We should now be able to run this executable and see our ``Hello, world!``::

    > ./_build/hello-world
    Hello, world!

More Sources
************

Modularizing our program is good, right? Let's do that.


Add a Header
************

Create a new subdirectory of ``src``, and we'll call it ``hello``::

> mkdir src/hello

Within this directory, create a ``strings.hpp``. Edit the content in your
editor of choice:

.. code-block:: c++
    :caption: ``<root>/src/hello/strings.hpp``
    :linenos:

    #ifndef HELLO_STRINGS_HPP_INCLUDED
    #define HELLO_STRINGS_HPP_INCLUDED

    #include <string>

    namespace hello {

    std::string get_greeting();

    }

    #endif


Change our ``main()``
*********************

Modify the content of ``<root>/src/hello-world.main.cpp`` to include our new
header and to use our ``get_greeting()`` function:

.. code-block:: c++
    :caption: ``<root>/src/hello-world.main.cpp``
    :linenos:
    :emphasize-lines: 1, 6

    #include <hello/strings.hpp>

    #include <iostream>

    int main() {
      std::cout << hello::get_greeting() << '\n';
    }


Compiling Again, and Linking...?
********************************

If you run the ``dds build`` command again, you will now see an error:

.. code-block:: text

    [12:55:25] [info ] [dds-hello] Link: hello-world
    [12:55:25] [info ] [dds-hello] Link: hello-world                    -     57ms
    [12:55:25] [error] Failed to link executable '<root>/_build/hello-world'.
    ...
    <additional lines follow>

The problem, of course, is that we've declared ``get_greeting`` to *exist*, but
be haven't *defined it*.


Adding Another Compiled Source
******************************

We'll add another compilable source file to our project. In the same
directory as ``strings.hpp``, add ``strings.cpp``:

.. code-block:: c++
    :caption: ``<root>/src/hello/strings.cpp``
    :linenos:

    #include <hello/strings.hpp>

    std::string hello::get_greeting() {
      return "Hello, world!";
    }


Compiling and Linking!
**********************

Run the ``dds build`` command again, and you'll find that the application
successfully compiles and links!

If you've used other build systems, you may have noticed a missing step: We
never told ``dds`` about our new source file. Actually, we never told ``dds``
about *any* of our source files. We never even told it the name of the
executable to generate. What gives?

It turns out, we *did* tell ``dds`` all of this information by simply placing
the files on the filesystem with the appropriate file paths. The name of the
executable, ``hello-world``, was inferred by stripping the trailing ``.main``
from the stem of the filename which defined the entry point.


Cleaning Up
***********

There's one final formality that should be taken care of before proceeding:
Creating a package manifest file.

``dds`` will work happily with packages that do not declare themselves, as long
as the filesystem structure is sufficient. However: To use features covered in
later tutorials, we'll need a simple ``package.dds`` file to declare
information about are package. This file should be placed directly in the
package root:

.. code-block:: yaml
    :caption: ``<root>/package.dds``

    Name: hello-dds
    Version: 0.1.0
    Namespace: tutorial


.. note::
    The ``Namespace`` option will be discussed later.

Rebuilding the project will show no difference at the moment.

.. seealso::
    Creating a single application executable is fine and all, but what if we
    want to create libraries? See the next page: :doc:`hello-lib`
