Remote Packages and Repositories
################################

.. highlight:: bash

|bpt| stores a local database of available packages, along with their
dependency statements and information about how a source distribution thereof
may be obtained.

Inside the database are *package repositories*, which are remote servers that
contain their own database of packages, and may also contain the packages
themselves. An arbitrary number of package repositories may be added to the
local database. When |bpt| updates its package information, it will download
the package database from each registered remote and import the listings into
its own local database, making them available for download.


Viewing Available Packages
**************************

The default catalog database is stored in a user-local location, and the
available packages can be listed with ``bpt pkg search``::

  $ bpt pkg search
      Name: abseil
  Versions: 2018.6.0, 2019.8.8, 2020.2.25
      From: repo-1.dds.pizza

      Name: asio
  Versions: 1.12.0, 1.12.1, 1.12.2, 1.13.0, 1.14.0, 1.14.1, 1.16.0, 1.16.1
      From: repo-1.dds.pizza

      Name: boost.leaf
  Versions: 0.1.0, 0.2.0, 0.2.1, 0.2.2, 0.2.3, 0.2.4, 0.2.5, 0.3.0
      From: repo-1.dds.pizza

      Name: boost.mp11
  Versions: 1.70.0, 1.71.0, 1.72.0, 1.73.0
      From: repo-1.dds.pizza

Optionally, one can search with a glob/fnmatch-style pattern::

  $ bpt pkg search 'neo-*'
      Name: neo-buffer
  Versions: 0.2.1, 0.3.0, 0.4.0, 0.4.1, 0.4.2
      From: repo-1.dds.pizza

      Name: neo-compress
  Versions: 0.1.0, 0.1.1, 0.2.0
      From: repo-1.dds.pizza

      Name: neo-concepts
  Versions: 0.2.2, 0.3.0, 0.3.1, 0.3.2, 0.4.0
      From: repo-1.dds.pizza


Remote Repositories
*******************

A remote package repository consists of an HTTP(S) server serving the following:

1. An accessible directory containing source distributions of various packages,
   and
2. An accessible database file that contains a listing of packages and some
   repository metadata.

The exact details of the directory layout and database are not covered here, and
are not necessary to make use of a repository.

When |bpt| uses a repository, it pulls down the database file and imports its
contents into its own local database, associating the imported package listings
with the remote repository which provides them. Pulling the entire database at
once allows |bpt| to perform much faster dependency resolution and reduces
the round-trips associated with using a dynamic package repository.


Adding a Repository
===================

Adding a remote repository to the local database is a simple single command::

  $ bpt pkg repo add "https://repo-1.dds.pizza"
  [info ] Pulling repository contents for repo-1.dds.pizza [https://repo-1.dds.pizza/]

This will tell |bpt| to add ``https://repo-1.dds.pizza`` as a remote
repository and immediately pull its package listings for later lookup. This
initial update can be suppressed with the ``--no-update`` flag.

.. note::

  After the initial ``pkg repo add``, the repository is *not* identified by its
  URL, but by its *name*, which is provided by the repository itself. The name
  is printed the first time it is added, and can be seen using ``pkg repo ls``.


Listing Repositories
====================

A list of package repositories can be seen with the ``pkg repo ls`` subcommand::

  $ bpt pkg repo ls


Removing Repositories
=====================

A repository can be removed by the ``pkg repo remove`` subcommand::

  $ bpt pkg repo remove <name>

Where ``<name>`` is given as the *name* (not URL!) of the repository.

**Note** that removing a repository will make all of its corresponding remote
packages unavailable, while packages that have been pulled into the local cache
will remain available even after removing a repository.


Updating Repository Data
========================

Repository data and package listings can be updated with the ``pkg repo update``
subcommand::

  $ bpt pkg repo update

This will pull down the databases of all registered remote repositories. If
|bpt| can detect that a repository's database is unchanged since a prior
update, that update will be skipped.


The Default Repository
**********************

When |bpt| first initializes its local package database, it will add a single
remote repository: ``https://repo-1.dds.pizza/``, which has the name
``repo-1.dds.pizza``. At the time of writing, this is the only official |bpt|
repository, and is populated sparsely with hand-curated and prepared packages.
In the future, the catalog of packages will grow and be partially automated.

There is nothing intrinsically special about this repository other than it being
the default when |bpt| first creates its package database. It can be removed
as any other, should one want tighter control over package availability.


Managing a Repository
*********************

A |bpt| repository is simply a directory of static files, so any HTTP server
that can serve from a filesystem can be used as a repository. |bpt| also
ships with a subcommand, ``repo``, that can be used to manage a repository
directory.


Initializing a Repository
=========================

Before anything can be done, a directory should be converted to a repository by
using ``repo init``::

  $ bpt repo init ./my-repo-dir --name=my-experimental-repo

This will add the basic metadata into ``./my-repo-dir`` such that |bpt| will
be able to pull package data from it.

The ``--name`` argument should be used to give the repository a unique name. The
name should be globally unique to avoid collisions: When |bpt| pulls a
repository that declares a given name, it will *replace* the package listings
associated with any repository of that name. As such, generic names like
``main`` or ``packages`` shouldn't be used in production.


Listing Contents
================

The packages in a repository can be listed using ``bpt repo ls <repo-dir>``.
This will simply print each package identifier that is present in the
repository.


Importing Source Distributions
==============================

If you have a source distribution archive, it can be imported with the
appropriately named ``bpt repo import`` command::

  $ bpt repo import ./my-repo some-pkg@1.2.3.tar.gz

Multiple archive paths may be provided to import them all at once.


Removing Packages
=================

A package can be removed from a repository with
``bpt repo remove <repo-dir> <pkg-id>``, where ``<pkg-id>`` is the
``<name>@<version>`` of the package to remove.
