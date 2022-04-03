How Do I Use Other Libraries as Dependencies?
#############################################

Of course, fundamental to any build system is the question of consuming
dependencies. |bpt| takes an approach that is both familiar and novel.

The *Familiar*:
  Dependencies are listed in a project's package manifest file
  (``package.json5``, for |bpt|).

  A range of acceptable versions is provided in the package manifest, which
  tells |bpt| and your consumers what versions of a particular dependency are
  allowed to be used with your package.

  Transitive dependencies are resolved and pulled the same as if they were
  listed in the manifest as well.

The *Novel*:
  |bpt| does not have a separate "install" step. Instead, whenever a ``bpt
  build`` is executed, the dependencies are resolved, downloaded, extracted,
  and compiled. Of course, |bpt| caches every step of this process, so you'll
  only see the download, extract, and compilation when you add a new dependency,

  Additionally, changes in the toolchain will necessitate that all the
  dependencies be re-compiled. Since the compilation of dependencies happens
  alongside the main project, the same caching layer that provides incremental
  compilation to your own project will be used to perform incremental
  compilation of your dependencies as well.

.. seealso:: :doc:`/guide/interdeps`


Listing Package Dependencies
****************************

Suppose you have a project and you wish to use
`spdlog <https://github.com/gabime/spdlog>`_ for your logging. To begin, we need
to find a ``spdlog`` package. We can search via ``bpt pkg search``::

  $ bpt pkg search spdlog
      Name: spdlog
  Versions: 1.4.0, 1.4.1, 1.4.2, 1.5.0, 1.6.0, 1.6.1, 1.7.0
      From: repo-1.dds.pizza
            No description

.. note::
  If you do not see any results, you may need to add the main repository to
  your package database. Refer to :doc:`/guide/remote-pkgs`.

In the output above, we can see one ``spdlog`` group with several available
versions. Let's pick the newest available, ``1.7.0``.

If you've followed at least the :doc:`Hello, World tutorial </tut/hello-world>`,
you should have at least a ``package.json5`` file present. Dependencies are
listed in the ``package.json5`` file under the ``depends`` key as an array of
dependency statement strings:

.. code-block:: js
  :emphasize-lines: 5-7

  {
    name: 'my-application',
    version: '1.2.3',
    namespace: 'myself',
    depends: [
      "spdlog^1.7.0"
    ]
  }

The string ``"spdlog^1.7.0"`` is a *dependency statement*, and says that we want
``spdlog``, with minimum version ``1.7.0``, but less than version ``2.0.0``.
Refer to :ref:`deps.ranges` for information on the version range syntax.

This is enough that |bpt| knows about our dependency, but there is another
step that we need to take:


Listing Usage Requirements
**************************

The ``depends`` is a package-level dependency, but we need to tell |bpt| that
we want to *use* a library from that package. For this, we need to provide a
``library.json5`` file alongside the ``package.json5`` file.

.. seealso::
  The ``library.json5`` file is discussed in :ref:`pkgs.libs` and
  :ref:`deps.lib-deps`.

We use the aptly-named ``uses`` key in ``library.json5`` to specify what
libraries we wish to use from our package dependencies. In this case, the
library from ``spdlog`` is named ``spdlog/spdlog``:

.. code-block:: js

  {
    name: 'my-application',
    uses: [
      'spdlog/spdlog'
    ]
  }


Using Dependencies
******************

We've prepared our ``package.json5`` and our ``library.json5``, so how do we get
the dependencies and use them in our application?

Simply *use them*. There is no separate "install" step. Write your application
as normal:

.. code-block:: cpp
  :caption: src/app.main.cpp

  #include <spdlog/spdlog.h>

  int main() {
    spdlog::info("Hello, dependency!");
  }

Now, when you run ``bpt build``, you'll see |bpt| automatically download
``spdlog`` *as well as* ``fmt`` (a dependency of ``spdlog``), and then build all
three components *simultaneously*. The result will be an ``app`` executable that
uses ``spdlog``.
