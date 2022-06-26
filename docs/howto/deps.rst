How Do I Use Other Libraries as Dependencies?
#############################################

Of course, fundamental to any build system is the question of consuming
dependencies. |bpt| takes an approach that is both familiar and novel.

The *Familiar*:

  Dependencies are listed in a project's manifest file (``bpt.yaml``, for
  |bpt|).

  A range of acceptable versions is provided in the project manifest, which
  tells |bpt| and your consumers what versions of a particular dependency are
  allowed to be used with your package.

  Transitive dependencies are resolved and pulled the same as if they were
  listed in the manifest as well.

The *Novel*:

  |bpt| does not have a separate "install" step. Instead, whenever a
  ``bpt build`` is executed, the dependencies are resolved, downloaded,
  extracted, and compiled. Of course, |bpt| caches every step of this process,
  so you'll only see the download, extract, and compilation when you add a new
  dependency,

  Additionally, changes in the toolchain will necessitate that all the
  dependencies be re-compiled. Since the compilation of dependencies happens
  alongside the main project, the same caching layer that provides incremental
  compilation to your own project will be used to perform incremental
  compilation of your dependencies as well.

.. seealso:: :doc:`/guide/deps`


Listing Package Dependencies
****************************

Suppose you have a project and you wish to use
`spdlog <https://github.com/gabime/spdlog>`_ for your logging. To begin, we need
to find a ``spdlog`` package. We can search via :ref:`cli.pkg-search`::

  $ bpt pkg search spdlog
      Name: spdlog
  Versions: 1.4.0, 1.4.1, 1.4.2, 1.5.0, 1.6.0, 1.6.1, 1.7.0, 1.8.0, 1.8.1,
            1.8.2, 1.8.3, 1.8.4, 1.8.5, 1.9.0, 1.9.1, 1.9.2
      From: https://repo-3.bpt.pizza/

In the output above, we can see one ``spdlog`` group with several available
versions. Let's pick the newest available, ``1.9.2``.

If you've followed at least the :doc:`Hello, World tutorial </tut/hello-world>`,
you should have at least a ``bpt.yaml`` file present. Dependencies are listed in
the ``bpt.yaml`` file under the ``dependencies`` key as a list of dependency
statement strings:

.. code-block:: yaml
  :caption: ``bpt.yaml``
  :emphasize-lines: 3,4

  name: my-application
  version: 1.2.3
  dependencies:
    - spdlog@1.9.2 using spdlog

The string ``"spdlog@1.9.2"`` is a *dependency statement*, and says that we want
``spdlog``, with minimum version ``1.9.2``, but less than version ``2.0.0``.
Refer to :ref:`deps.ranges` for information on the version range syntax.

The ``using spdlog`` suffix declares that we want to use the ``spdlog``
libraries within the package. This will attach the ``spdlog/spdlog`` library to
the libraries within our own project.


Using Dependencies
******************

We've prepared our ``bpt.yaml`` so how do we get the dependencies and use them
in our code?

Simply *use them*. There is no separate "install" step. Write your application
as normal:

.. code-block:: cpp
  :caption: ``src/app.main.cpp``

  #include <spdlog/spdlog.h>

  int main() {
    spdlog::info("Hello, dependency!");
  }

Now when you run ``bpt build``, you'll see |bpt| automatically download
``spdlog`` *as well as* ``fmt`` (a dependency of ``spdlog``), and then build all
three components *simultaneously*. The result will be an ``app`` executable that
uses ``spdlog``.
