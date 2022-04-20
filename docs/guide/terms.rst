#########################
Miscellaneous Terminology
#########################

.. default-role:: term

This page documents miscellaneous terminology used throughout |bpt| and `CRS`.

.. glossary::

  JSON

    JSON is the *JavaScript Object Notation*, a plaintext format for
    representing semi-structured data.

    JSON values can be strings, numbers, boolean values, a ``null`` value,
    arrays of values, and "objects" which map a string to another JSON value.

    .. rubric:: Example

    .. code-block:: json
      :caption: ``example.json``

      {
        "foo": 123.0,
        "bar": [true, false, null],
        "baz": "quux",
        "another": {
          "nested-object": [],
        }
      }

    JSON data cannot contain comments. The order of keys within a JSON object is
    not significant.


  YAML

    YAML is a data representation format intended to be written by humans and
    contain arbitrarily nested data structures.

    YAML is a superset of `JSON`, so every valid JSON document is also a valid
    equivalent YAML document.

    You can learn more about YAML at https://yaml.org.

    The example from the `JSON` definition could also be written in YAML as:

    .. code-block:: yaml
      :caption: ``example.yaml``

      foo: 123.0
      bar:
        - true
        - false
        - null
      # We can use comments in YAML!
      baz: quux
      another:
        nested-object: []

    YAML can also be written as a "better JSON" by using the data block
    delimiters:

    .. code-block:: yaml
      :caption: ``example.yaml``

      {
        foo: 123.0,
        bar: [true, false, null],
        # We can still use comments!
        baz: "quux",
        another: {nested-object: []},  # Trailing commas are allowed!
      }

  tweak-headers

    Special `header files <header file>` that are used to inject configuration
    options into dependency libraries. The library to be configured must be
    written to support tweak-headers.

    Tweak headers are placed in the "tweaks directory", which is controlled with
    the :option:`bpt build --tweaks-dir <--tweaks-dir>` option given to |bpt|
    build commands.

    .. seealso:: For more information, `refer to this article`__.

    __ https://vector-of-bool.github.io/2020/10/04/lib-configuration.html

  URL

    A **U**\ niform **R**\ esource **L**\ ocator is a string that specifies how to
    find a resource, either on the network/internet or on the local filesystem.

  environment variables

    Every operating system process has a set of *environment variables*, which
    is an array of key-value pairs that map a text string key to some text
    string value. These are commonly used to control the behavior of commands
    and subprocesses.

    For example, the "``PATH``" environment variable controls how `command`
    names are mapped to executable files.

    |bpt| uses some environment variables to control some behavior, such as
    :envvar:`BPT_LOG_LEVEL` and :envvar:`BPT_NO_DEFAULT_REPO`.
