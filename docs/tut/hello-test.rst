A *Hello, World* Test
#####################

.. default-role:: term

So far, we have a simple library with a single function: ``get_greeting()`` and
an application that makes use of it. How can we test it?

With |bpt|, similar to generating applications, creating a test requires adding
a suffix to a source `filename stem <file stem>`. Instead of ``.main``, simply
add ``.test`` before the `file extension`.


A New Test Executable
*********************

We'll create a test for our ``strings`` component, in a file named
``strings.test.cpp``.

.. code-block:: c++
    :caption: ``<root>/src/hello/strings.test.cpp``
    :linenos:

    #include <hello/strings.hpp>

    int main() {
      if (hello::get_greeting() != "Hello world!") {
        return 1;
      }
      return 0;
    }

If you run ``bpt build`` once again, |bpt| will generate a test executable and
run it immediately. If the test executable exits with a non-zero exit code, then
it will consider the test to have failed, and |bpt| itself will exit with a
non-zero exit code.

.. important::
    |bpt| executes tests *in parallel* by default! If the tests need access
    to a shared resource, locking must be implemented manually, or the shared
    resource should be split.

.. note::
    |bpt| builds and executes tests for *every build* **by default**. The
    ``*.test.cpp`` tests are meant to be very fast *unit* tests, so consider
    their execution time carefully.

If your code matches the examples so far, the above test will *fail*. Keen eyes
will already know the problem, but wouldn't it be better if we had better test
diagnostics?


Using a Library for Testing
***************************

A library in |bpt| can declare dependencies on other libraries, possibly in
other packages. One can also declare that a dependency is "for testing" rather
than being a strong dependency of the library itself. A "for test" library
dependency will only be available within the ``.test.*`` source files, and
downstream library consumers will not see the dependency.

To declare dependencies, we return to the ``bpt.yaml`` file. We'll declare a
dependency on the `Catch2`_ C and C++ unit testing framework:

.. _Catch2: https://github.com/catchorg/Catch2

.. code-block:: yaml
  :caption: ``<root>/bpt.yaml``
  :emphasize-lines: 3,4

  name: hello-bpt
  version: 0.1.0
  test-dependencies:
    - catch2@2.13.7 using main

The dependency declaration here is using a special shorthand string to declare
how we wish to consume the dependency. The shorthand string here says that we
depend on "``catch2``" version ``2.13.7`` (or any compatible version), and we
are "using" a library from the package called "``main``". This library in Catch2
defines a ``main()`` function on our behalf that will run all the tests defined
in our test `source file`. Adding the dependency under the ``test-dependencies``
key means that the dependency is only loaded for testing purposes and doesn't
propagate to our own users.

If you now run ``bpt build``, we will likely get a linker error for a
multiply-defined ``main`` function. The ``catch2/main`` library contains its own
definition of the ``main()`` function. This means we cannot provide our own
``main`` function, and should instead use Catch's ``TEST_CASE`` macro to declare
our test cases.

In addition to an entrypoint, we can now use the Catch2 headers in our tests,
simply by ``#include``-ing the appropriate path. We'll modify our test to use
the Catch2 test macros instead of our own logic. We'll leave the condition the
same, though:

.. code-block:: c++
    :caption: ``<root>/src/hello/strings.test.cpp``
    :emphasize-lines: 3, 5-7

    #include <hello/strings.hpp>

    #include <catch2/catch.hpp>

    TEST_CASE("Check the greeting") {
      CHECK(hello::get_greeting() == "Hello world!");
    }

Now running ``bpt build`` will print more output that Catch has generated as
part of test execution, and we can see the reason for the failing test::

    [error] Test <root>/_build/test/hello/strings failed! Output:

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

    [bpt - test output end]

Now that we have the direct results of the offending expression, we can much
more easily diagnose the nature of the test failure. In this case, the function
returns a string containing a comma ``,`` while our expectation lacks one. If we
fix either the ``get_greeting`` or the expected string, we will then see our
tests pass successfully and |bpt| will exit cleanly.
