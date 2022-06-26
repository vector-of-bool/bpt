Remote Packages and Repositories
################################

.. |--use-repo| replace:: :option:`--use-repo <bpt build --use-repo>`
.. |--no-default-repo| replace:: :option:`--no-default-repo`

Unlike most package management systems, |bpt| does not have a separate "install"
step for downloading and using dependencies. Instead each |bpt| operation "uses"
some set of repositories, and data from these repositories are pulled and cached
on-the-fly as required.

The repositories in use by any operation are specified with the |--use-repo|
option, which is accepted by any subcommand that can use package dependencies.
|--use-repo| can be specified any number of times on the command line, and |bpt|
will treat all of the repositories as a pool of packages available for
dependency resolution.

.. note::

  In addition to any repositories enabled with |--use-repo|, |bpt| will *by
  default* use a default repository at https://repo-3.bpt.pizza/. This can be
  disabled with the |--no-default-repo| option.

.. important::

  The local package cache **is not** part of the interface and standard
  workflow. |bpt| does not "remember" the given |--use-repo| values provided for
  a build. Instead, each invocation of |bpt| is considered independent, and
  *only* the packages that are available in the repositories specified with
  |--use-repo| will be available for dependency resolution. Locally cached
  packages are not relevant for the purposes of dependency resolution.

.. note::

  The metadata from package repositories is pulled and updated on-the-fly. |bpt|
  uses the HTTP Cache-Control__, ETag__, and Last-Modified__ headers to
  determine when its cached copy of repository metadata is out-of-date.

  __ https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cache-Control
  __ https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/ETag
  __ https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Last-Modified


Viewing Available Packages
**************************

To discover which packages and which versions are available the listing can be
obtained with the :ref:`cli.pkg-search` subcommand::

  $ bpt pkg search

      Name: abseil
  Versions: 2018.6.0, 2019.8.8, 2020.2.25
      From: repo-1.bpt.pizza

      Name: asio
  Versions: 1.12.0, 1.12.1, 1.12.2, 1.13.0, 1.14.0, 1.14.1, 1.16.0, 1.16.1
      From: repo-1.bpt.pizza

      Name: boost.leaf
  Versions: 0.1.0, 0.2.0, 0.2.1, 0.2.2, 0.2.3, 0.2.4, 0.2.5, 0.3.0
      From: repo-1.bpt.pizza

      Name: boost.mp11
  Versions: 1.70.0, 1.71.0, 1.72.0, 1.73.0
      From: repo-1.bpt.pizza

Optionally, one can search with a fnmatch-style pattern::

  $ bpt pkg search 'neo-*'

      Name: neo-buffer
  Versions: 0.2.1, 0.3.0, 0.4.0, 0.4.1, 0.4.2
      From: repo-1.bpt.pizza

      Name: neo-compress
  Versions: 0.1.0, 0.1.1, 0.2.0
      From: repo-1.bpt.pizza

      Name: neo-concepts
  Versions: 0.2.2, 0.3.0, 0.3.1, 0.3.2, 0.4.0
      From: repo-1.bpt.pizza


.. note::

  The search packages will include all repositories enabled for the
  ``pkg search`` invocation, including the default repository *unless*
  |--no-default-repo| is specified.

  To search packages available in a specific repository *without* including
  packages from the default repository, pass |--use-repo| along with
  |--no-default-repo|::

    $ bpt pkg search --use-repo=my-repo.example.org --no-default-repo


The Default Repository
**********************

The *default repository* is the repository that is always enabled by |bpt|
*unless* |--no-default-repo| is specified. The default repository (at time of
writing) lives at https://repo-3.bpt.pizza/.

There is nothing intrinsically special about this repository other than it being
the default for |bpt|. It can be disable with |--no-default-repo| or
:envvar:`BPT_NO_DEFAULT_REPO` should one want tighter control over package
availability.
