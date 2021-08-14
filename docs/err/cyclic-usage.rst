Error: Usage Requirements Cycle
###############################

A library can declare that it *uses* another library by using the ``uses`` key
in ``library.json5``.

These ``uses`` requirements must form a directed acyclic graph, meaning that
there can be no dependency cycle. For example, if library ``clover`` lists
library ``beehive`` in its ``uses`` list, then ``beehive`` cannot also list
``clover``---even transitively---else there would be a dependency cycle.

To fix this issue, you must remove the dependency cycle. Oftentimes, this can be
done by splitting one of the libraries into two. For example, ``beehive`` can
become ``pollinator + beehive`` with ``beehive`` uses ``pollinator`` and
``clover`` uses ``pollinator``.

Also, note that this issue also occurs when a library *uses* itself. You can
resolve that issue by removing the self reference; libraries already implicitly
have access to themselves.