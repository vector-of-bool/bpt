A *Hello, World* Test
#####################

So far, we have a simple library with a single function: ``get_greeting()``
and an application that makes use of it. How can we test it?

With ``dds``, similar to generating applications, creating a test requires
adding a suffix to a source filename stem. Instead of ``.main``, simply
add ``.test`` before the file extension.


A New Test Executable
*********************

We'll create a test for our ``strings`` component, in a file named
``strings.test.cpp``. We'll use an ``assert`` to check our ``get_greeting()``
function:

.. code-block:: c++
    :caption: ``<root>/src/hello/strings.test.cpp``
    :linenos:

    #include <hello/strings.hpp>

    int main() {
      if (hello::get_greeting() != "Hello world!") {
        return 1;
      }
    }

If you run ``dds build`` once again, ``dds`` will generate a test executable
and run it immediately. If the test executable exits with a non-zero exit code,
then it will consider the test to have failed, and ``dds`` itself will exit
with a non-zero exit code.

.. important::
    ``dds`` executes tests *in parallel* by default! If the tests need access
    to a shared resource, locking must be implemented manually, or the shared
    resource should be split.

.. note::
    ``dds`` builds and executes tests for *every build* **by default**. The
    ``*.test.cpp`` tests are meant to be very fast *unit* tests, so consider
    their execution time carefully.

If your code matches the examples so far, the above test will *fail*. Keen eyes
will already know the problem, but wouldn't it be better if we had better test
diagnostics?


A ``Test-Driver``: Using *Catch2*
*********************************

``dds`` ships with built-in support for the `Catch2`_ C and C++ testing
framework.

.. _catch2: https://github.com/catchorg/Catch2

To make use of Catch as our test driver, we simply declare this intent in the
``package.dds`` file at the package root:

.. code-block:: yaml
    :caption: ``<root>/package.dds``
    :emphasize-lines: 5

    Name: hello-dds
    Version: 0.1.0
    Namespace: tutorial

    Test-Driver: Catch-Main

If you now run ``dds build``, we will get a linker error for a multiply-defined
``main`` function. When setting the ``Test-Driver`` to ``Catch-Main``, ``dds``
will compile an entrypoint separately from any particular test, and the tests
will link against that entrypoint. This means we cannot provide our own
``main`` function, and should instead use Catch's ``TEST_CASE`` macro to
declare our test cases.

In addition to an entrypoint, ``dds`` provides a ``catch.hpp`` header that we
may use in our tests, simply by ``#include``-ing the appropriate path. We'll
modify our test to use the Catch test macros instead of our own logic. We'll
leave the condition the same, though:

.. code-block:: c++
    :caption: ``<root>/src/hello/strings.test.cpp``
    :linenos:
    :emphasize-lines: 3, 5-7

    #include <hello/strings.hpp>

    #include <catch2/catch.hpp>

    TEST_CASE("Check the greeting") {
      CHECK(hello::get_greeting() == "Hello world!");
    }

Now running ``dds build`` will print more output that Catch has generated as
part of test execution, and we can see the reason for the failing test::

    [16:41:45] [error] Test <root>/_build/test/hello/strings failed! Output:

    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    strings is a Catch v2.10.2 host application.
    Run with -? for options

    -------------------------------------------------------------------------------
    Check the greeting
    -------------------------------------------------------------------------------
    <root>/src/hello/strings.test.cpp:5
    ...............................................................................

    <root>/src/hello/strings.test.cpp:5: FAILED:
      CHECK( hello::get_greeting() == "Hello world!" )
    with expansion:
      "Hello, world!" == "Hello world!"

    ===============================================================================
    test cases: 1 | 1 failed
    assertions: 1 | 1 failed

    [dds - test output end]

Now that we have the direct results of the offending expression, we can
much more easily diagnose the nature of the test failure. In this case, the
function returns a string containing a comma ``,`` while our expectation lacks
one. If we fix either the ``get_greeting`` or the expected string, we will then
see our tests pass successfully and ``dds`` will exit cleanly.
