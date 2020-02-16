Error: Invalid package filesystem
#################################

``dds`` prescribes a filesystem structure that must be followed for it to
build and process packages and libraries. This structure isn't overly strict,
and is thoroughly explained on the :doc:`/guide/packages` page.

For exporting/generating a source distribution from a package, the *package
root* requires a ``package.json5`` file and each *library root* requires a
``library.json5`` file.

..  .
  TODO: Create are more detailed reference page for package and library layout,
  and include those links in a `seealso`.
