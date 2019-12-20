Error: Git requires both a URL and a ref to clone
#################################################

This error occurs when attempting to add an entry to the package catalog that
uses the ``Git`` acquisition method.

When ``dds`` obtains a package from the catalog using the ``Git`` method, it
needs a URL to clone from, and a Git ref to clone. ``dds`` uses a technique
known as "shallow cloning," which requires a known Git reference to clone from.
The reference may be a tag, branch, or an individual commit (Using a Git commit
as the ``ref`` requires support from the remote Git server, and it is often
unavailable in most setups). Using a Git tag is strongly recommended.

.. seealso::
    Refer to the documentation on :doc:`/guide/catalog`.