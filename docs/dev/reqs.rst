Supported Platforms and Build Requirements
##########################################

``dds`` aims to be as cross-platform as possible. It currently build and
executes on **Windows**, **macOS**, **Linux**, and **FreeBSD**. Support for
additional platforms is possible but will require modifications to
``bootstrap.py`` that will allow it to be built on such platforms.


Build Requirements
******************

Building ``dds`` has a simple set of requirements:

- **Python 3.6** or newer to run the bootstrap/CI scripts.
- A C++ compiler that has rudimentary support for several C++20 features,
  including Concepts. Newer releases of Visual C++ that ship with **VS
  2019** will be sufficient on Windows, as will **GCC 9** with ``-fconcepts`` on
  other platforms.

.. note::
    On Windows, you will need to execute the build from within a Visual C++
    enabled environment. This may involve launching the build from a Visual
    Studio Command Prompt.

.. note::
    At the time of writing, C++20 Concepts has not yet been released in Clang,
    but should be available in LLVM/Clang 11 and newer.

