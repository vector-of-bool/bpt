#####
Names
#####

.. default-role:: term

.. glossary::

   name

      In `CRS` and |bpt| a *name* refers to a textual identifier matching a
      specific form.

      A valid *name* must match the following requirements:

      1. Must begin with a lowercase ASCII alphabetic character
      2. Must not contain any capital letter characters.
      3. May only contain ASCII alphanumeric characters and the ASCII dot
         "``.``", underscore ":literal:`_`", and hyphen "``-``".
      4. Must not contain any adjacent punctuation characters.
      5. Must end with a letter or digit.

A CRS name can be validated by testing against the following regular
expression

.. code-block:: text

    ^([a-z][a-z0-9]*)([._-][a-z0-9]+)*$

It checks the following rules:

1. A name must begin with a lowercase ASCII alphabetic character (Must match
   ``^[a-z]``).

#. A name may not be an empty string.
#. A name may not contain any capital letter characters.
#. A name may only contain ASCII alphanumeric characters and the ASCII dot
   "``.``", underscore ":literal:`_`", and hyphen "``-``".

#. A name may not contain any adjacent punctuation characters.
#. A name must end with a letter or a digit.

These name restrictions are currently in place to simplify path handling and
prevent havok from filepath normalization on certain filesystems. Future
revisions will likely loosen these requirements.

Examples
########

.. |yes| replace:: ✔️
.. |no| replace:: ❌

.. csv-table::
   :align: left
   :widths: auto

   Name string, Is valid?, Reason

   .. default-role:: code

   `foo.bar`, |yes|,
   `my-library.baz`, |yes|,
   `foo_bar`, |yes|,
   `somename`, |yes|,
   `something-else`, |yes|,
   `foo.`, |no|, Names may not end with a punctuation character
   `Foo`, |no|, Names may not contain capital letters
   `4g`, |no|, Names may not begin with a digit
   `_mylibrary`, |no|, Names may not begin with underscores
   `foo__bar`, |no|, Names may not contain adjacent punctuation or underscores
   `foo..bar`, |no|, ‘‘
   `my-pkg.SomeLibrary`, |no|, Names may not contain capital letters.

.. default-role:: any
