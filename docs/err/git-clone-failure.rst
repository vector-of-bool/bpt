Error: A Git ``clone`` operation failed
#######################################

This error indicates that ``dds`` failed to clone a Git repository.

``dds`` will invoke ``git clone`` as a subprocess to retrieve a copy of a
remote repository. There are several reasons this might fail, but it is best
to refer to the output of the ``git`` subprocess to diagnose the issue.

A non-exhaustive list of things to check:

#. Is the ``git`` executable available and on the ``PATH`` environment variable?
#. Is the URL to the repository correct?
#. Is the remote server accessible?
#. Do you have read-access to the repository in question?
#. Does the named tag/branch exist in the remote?
#. If cloning a specific Git revision, does the remote server support cloning
   a repository by a specific commit? (Very often Git servers are not
   configured to support this capability).

Be aware if you are using SSH-style ``git-clone`` that it will require the
correct SSH keys to be available on the system where ``dds`` is running.