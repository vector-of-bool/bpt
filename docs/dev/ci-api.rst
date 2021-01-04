DDS CI Scripts Python API
#########################

Types from pytest
*****************

These types are defined by pytest, but are used extensively within the testing
scripts.

.. class:: _pytest.fixtures.FixtureRequest

  .. seealso:: :class:`pytest.FixtureRequest`

.. class:: _pytest.tmpdir.TempPathFactory

  .. seealso:: :class:`pytest.TempPathFactory`


Test Fixtures
*************

The following test fixtures are defined:

- :func:`~dds_ci.testing.fixtures.dds` - :class:`dds_ci.dds.DDSWrapper` - A
  wrapper around the ``dds`` executable under test.
- :func:`~dds_ci.testing.fixtures.tmp_project` -
  :class:`dds_ci.testing.fixtures.Project` - Create a new empty directory to be
  used as a test project for ``dds`` to execute.
- :func:`~dds_ci.testing.http.http_repo` -
  :class:`dds_ci.testing.http.RepoServer` - Create a new dds repository and
  spawn an HTTP server to serve it.

Module: ``dds_ci``
******************

.. automodule:: dds_ci
  :members:


Module: ``dds_ci.dds``
**********************

.. automodule:: dds_ci.dds
  :members:


Module: ``dds_ci.proc``
***********************

.. automodule:: dds_ci.proc
  :members:


Module: ``dds_ci.testing``
**************************

.. automodule:: dds_ci.testing
  :members:


Module: ``dds_ci.testing.http``
*******************************

.. automodule:: dds_ci.testing.http
  :members:


Module: ``dds_ci.testing.fixtures``
***********************************

.. automodule:: dds_ci.testing.fixtures
  :members:


Module: ``dds_ci.testing.error``
********************************

.. automodule:: dds_ci.testing.error
  :members:
