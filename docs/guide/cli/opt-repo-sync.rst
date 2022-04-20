.. option:: --repo-sync {always,cached-okay,never}

    Mode for repository metadata synchronization. Default is ``always``.

    ``always``
        Always try to keep the repository metadata up-to-date. If there is a
        failure to validate/update the repo metadata, the build fails
        immediately.

    ``cached-okay``
        Attempt to update the repository metadata. If a low-level network error
        prevents such an update and we have cached metadata for the repository,
        ignore the error and continue.

    ``never``
        Do not try to update the repository metadata. This option requires that
        there be a local cache of the repository metadata before being used.

        If any packages need to be pulled for the build and those packages are
        not already locally cached, |bpt| will still attempt to download those
        packages from the repository.

    Regardless of this option, the HTTP ``Cache-Control`` headers for the
    repository data will be respected. If repository data is within the lifetime
    of the ``max-age`` of the resource, |bpt| will not try to update the
    repository data.
