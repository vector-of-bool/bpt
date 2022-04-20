A *Hello, world!* Application
#############################

.. default-role:: term

Creating a *hello world* application is very simple.

Creating a New Project
**********************

Creating a very basic project can be done with the ``bpt new`` `command`:

.. code-block:: bash

    $ bpt new hello-bpt

|bpt| will ask you a few questions to begin. Accept the defaults for now. This
will create a new project named "``hello-bpt``" in a new `directory` also named
"``hello-bpt/``". (By default, ``bpt new`` will create the project in a
`subdirectory` with the same name as the project itself.)

.. seealso:: The subcommand documentation: :doc:`/guide/cli/new`

From here on, the `directory` of the ``hello-bpt/`` created by this `command`
will simply be referred to as ``<root>``.


The New Project Files
*********************

Within the new project `directory`, ``bpt`` will create a ``bpt.yaml`` and a
``src/`` directory. (If you chose to split headers and sources, it will also
create an ``include/`` directory.)

The ``bpt.yaml`` declares the information about the project that |bpt| will use
to build, package, test, and distribute the project.

The ``src/`` (and possible ``include/``) `subdirectories <subdirectory>` are
used to contain the `source code` of the project. |bpt| will search for and
compile `source files <source file>` that you add to these directories,
including any nested subdirectories of those top-level directories. The ``src/``
and ``include/`` directories are referred to as `source roots <source root>`.

The default project created by ``bpt new`` will contain a single source file and
a single header file. If you accepted the defaults of ``bpt new hello-bpt``,
these will be:

- ``src/hello-bpt/hello-bpt.hpp`` - A simple C++ `header file`
- ``src/hello-bpt/hello-bpt.cpp`` - A single C++ `source file` that contains
  uses the header file.

We won't use these source files just yet.


Creating an Application Entrypoint
**********************************

To add a `source file` to our project, we simply create a file within a
`source root` with the appropriate `file extension`. Our source root is
``<root>/src/``, so we'll place a source file in there. In addition, because we
want to create an *application* we need to designate that the source file
provides an application *entry point*, i.e. a ``main()`` function. To do this,
we simply prepend ``.main`` to the file extension. Create a file::

> <root>/src/hello-app.main.cpp

and open it in your editor of choice. We'll add the classic C++ *hello, world*
program:

.. code-block:: c++
    :linenos:
    :caption: ``<root>/src/hello-app.main.cpp``

    #include <iostream>

    int main() {
      std::cout << "Hello, world!\n";
    }


Building *Hello, World*
***********************

Now comes the fun part. It is time to actually compile the application!

.. important::

    If you intend to compile with Visual C++, the build must be executed from
    within a Visual Studio or Visual C++ development command prompt. These
    program shortcuts should be made available with any standard installation of
    the Visual C++ toolchain.

    |bpt| **will not** automatically load the Visual C++ environment.

To build the program, we must provide |bpt| with information about our program
toolchain. |bpt| comes with a few "built in" toolchain options that can be
used out-of-the-box, and they'll be suitable for our purposes.

- If you are compiling with GCC, the toolchain name is ``:gcc``.
- If you are compiling with Clang, the toolchain name is ``:clang``.
- If you are compiling with Microsoft Visual C++, the toolchain name is
  ``:msvc``.

.. note::
    The leading colon ``:`` is important: This tells |bpt| to use its
    built-in toolchain information rather than looking for a toolchain file of
    that name.

To execute the build, run the :doc:`/guide/cli/build` command as in the
following example, providing the appropriate toolchain name in place of
``<toolchain>``::

> bpt build -t <toolchain>

For example, if you are using ``gcc``, you would run the command as::

> bpt build -t :gcc

If all is successful, |bpt| will emit information about the compile and link
process, and then exit without error.

By default, build results will be placed in a `subdirectory` of the package root
named ``_build``. Within this directory, you will find the generated executable
named ``hello-app`` (with a ``.exe`` suffix if on Windows).

We should now be able to run this executable and see our ``Hello, world!``::

    > ./_build/hello-app
    Hello, world!


Using Our Headers
*****************

``bpt new`` created a default header and source file for our projet, but we
aren't using them in our application yet. This can be done by adding an
``#include`` directive to the application's `source file`:

.. code-block:: c++
    :caption: ``src/hello-app.main.cpp``
    :linenos:
    :emphasize-lines: 1

    #include <hello-bpt/hello-bpt.hpp>

    #include <iostream>

    int main() {
      std::cout << "Hello, world!\n";
    }

The `relative filepath` between the angle brackets of the ``#include`` directive
is resolved relative to the `source root` directory.

.. note::

    For detailed information on ``#include`` resolution, refer to information
    about the `header search path`.

This change will ``#include`` our `header file`, but it doesn't make use of it
yet. Since we have included the file, we will now be able to refer to entities
that are declared/defined within it. The default header contains a single
function: ``int hello_bpt::the_answer()``. We can call it and print the return
value to ``std::cout``:

.. code-block:: c++
    :caption: ``src/hello-app.main.cpp``
    :linenos:
    :emphasize-lines: 6,7,8

    #include <hello-bpt/hello-bpt.hpp>

    #include <iostream>

    int main() {
      std::cout << "The answer is: "
                << hello_bpt::the_answer()
                << '\n';
    }

We can now ``bpt build`` our program again and see the output::

    > bpt build -t <toolchain>
    # [... elided ...]
    > ./_build/hello-app
    The answer is: 42

More Sources
************

Inevitably, we'll want more source files to subdivide our program into
easy-to-understand chunks. Adding source files is easy with |bpt|!


Add a Header
************

Create a new `subdirectory` of ``src/``. We'll call it ``hello``::

> mkdir src/hello

Within this directory, create a ``strings.hpp`` `header file`. Edit the content
in your editor of choice:

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

Modify the content of ``<root>/src/hello-app.main.cpp`` to include our new
header and to use our ``get_greeting()`` function:

.. code-block:: c++
    :caption: ``<root>/src/hello-app.main.cpp``
    :linenos:
    :emphasize-lines: 1, 6

    #include <hello/strings.hpp>

    #include <iostream>

    int main() {
      std::cout << hello::get_greeting() << '\n';
    }


Compiling Again, and Linking...?
********************************

If you run the ``bpt build`` command again, you will now see an error:

.. code-block:: text

    [info ] [bpt-hello] Link: hello-app
    [info ] [bpt-hello] Link: hello-app                    -     57ms
    [error] Failed to link executable '<root>/_build/hello-app'.
    ...
    <additional lines follow>

The problem, of course, is that we've declared ``get_greeting`` to *exist*, but
be haven't *defined it*.


Adding Another Compiled Source
******************************

We'll add another compilable `source file` to our project. In the same
`directory` as ``strings.hpp``, add ``strings.cpp``:

.. code-block:: c++
    :caption: ``<root>/src/hello/strings.cpp``
    :linenos:

    #include <hello/strings.hpp>

    std::string hello::get_greeting() {
      return "Hello, world!";
    }


Compiling and Linking!
**********************

Run the ``bpt build`` command again, and you'll find that the application
successfully compiles and links!

If you've used other build systems, you may have noticed a missing step: We
never told |bpt| about our new `source file`. Actually, we never told |bpt|
about *any* of our source files. We never even told it the name of the
executable to generate. What gives?

It turns out, we *did* tell |bpt| all of this information by simply placing the
files on the filesystem with the appropriate filepaths. The name of the
executable, ``hello-app``, was inferred by stripping the trailing ``.main`` from
the `stem <file stem>` of the `filepath` of the `source file` which defined the
entry point.

.. seealso::
    Creating a single application executable is fine and all, but what if we
    want to create libraries? See the next page: :doc:`hello-lib`
