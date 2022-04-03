A *Hello, World* Library
########################

Creating a single application is fun, but what if we want to create code that we
can distribute and reuse across other packages and projects? That's what
*libraries* are for.

You may notice something strange: This page is much shorter than the previous:
It turns out that we've already covered all the material to make a library in
the page on creating a :doc:`Hello, world! executable <hello-world>`. As soon as
we created the ``strings.hpp`` file, our project had become a library. When we
added a ``strings.cpp`` file to accompany it, our library became a *compiled*
library.

The ``hello-world.main.cpp`` file is an application source file associated with
the library but it will not be linked with the library itself, as it is
specifically designated to be an application entry point.

Before continuing on, note the following about creating a library that wasn't
specifically addressed in the prior example:

#. The *source roots* of a library are added to the compiler's ``#include``
   search-path. In our example, this is only the ``src/`` directory of the
   project.
#. |bpt| also supports a top-level directory named ``include/``. Both
   ``include/`` and ``src/`` may be present in a single library, but there are
   some important differences. Refer to :ref:`pkgs.lib-roots` in the layout
   guide.
#. A single *library root* may contain any number of applications defined in
   ``.main`` files, but a *library root* will correspond to exactly *one*
   library. Defining additional libraries requires additional packages or
   adding multiple library roots to a single package.

.. seealso::

    Like flossing, we all know we *should* be writing tests, but it can be such
    a hassle. Fortunately, |bpt| makes it simple. Read on to:
    :doc:`hello-test`.
