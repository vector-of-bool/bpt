Supported Platforms and Build Requirements
##########################################

|bpt| aims to be as cross-platform as possible. It currently build and executes
on **Windows**, **macOS**, **Linux**, and **FreeBSD**. Support for additional
platforms is possible but will require modifications to ``bootstrap.py`` that
will allow it to be built on such platforms.


Build Requirements
******************

Building |bpt| has a simple set of requirements:

- **Python 3.7** or newer to run the bootstrap/CI scripts.
- A C++ compiler that has rudimentary support for several C++20 features,
  including Concepts. Newer releases of Visual C++ that ship with **VS 2019**
  will be sufficient on Windows, as will **GCC 10** with ``-std=c++20`` and
  ``-fcoroutines`` on other platforms.
- On Linux, the OpenSSL headers are needed. This can be installed through the
  ``libssl-dev`` package on Debian/Ubuntu or ``openss-devel`` +
  ``openssl-static`` on RedHat systems. (For Windows, the |bpt| repository ships
  with a bundled OpenSSL).

.. note::
    On Windows, you will need to execute the build from within a Visual C++
    enabled environment. This may involve launching the build from a Visual
    Studio Command Prompt.
