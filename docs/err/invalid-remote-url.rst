Error: Invalid Remote/Package URL
#################################

``dds`` encodes a lot of information about remotes repositories and remote
packages in URLs. If you received this error, it may be because:

1. The URL syntax is invalid. Make sure that you have spelled it correctly.
2. The URL scheme (the part at the beginning, before the ``://``) is unsupported
   by ``dds``. ``dds`` only supports a subset of possible URL schemes in
   different contexts. Check the output carefully and read the documentation
   about the task you are trying to solve.
3. There are missing URL components that the task is expecting. For example,
   ``git`` remote URLs require that the URL have a URL fragment specifying the
   tag/branch to clone. (The fragment is the final ``#`` component.)
