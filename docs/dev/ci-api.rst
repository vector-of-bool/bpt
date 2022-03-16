DDS CI Scripts Python API
#########################

Test Fixtures
*************

The following test fixtures are defined:

- :func:`~dds_ci.testing.fixtures.dds` - :class:`dds_ci.dds.DDSWrapper` - A
  wrapper around the ``dds`` executable under test.
- :func:`~dds_ci.testing.fixtures.tmp_project` -
  :class:`dds_ci.testing.fixtures.Project` - Create a new empty directory to be
  used as a test project for ``dds`` to execute.
- :func:`~dds_ci.testing.repo.http_crs_repo` -
  :class:`dds_ci.testing.repo.CRSRepoServer` - Create a new dds repository and
  spawn an HTTP server to serve it.


Module: ``dds_ci``
******************

.. automodule:: dds_ci
  :members:



Module: ``dds_ci.bootstrap``
****************************

.. automodule:: dds_ci.bootstrap
  :members:


Module: ``dds_ci.dds``
**********************

.. automodule:: dds_ci.dds
  :members:


Module: ``dds_ci.docs``
***********************

.. automodule:: dds_ci.docs
  :members:


Module: ``dds_ci.msvs``
***********************

.. automodule:: dds_ci.msvs
  :members:


Module: ``dds_ci.paths``
************************

.. automodule:: dds_ci.paths
  :members:


Module: ``dds_ci.proc``
***********************

.. automodule:: dds_ci.proc
  :members:


Module: ``dds_ci.tasks``
************************

.. automodule:: dds_ci.tasks
  :members:


Module: ``dds_ci.util``
***********************

.. automodule:: dds_ci.util
  :members:


Module: ``dds_ci.toolchain``
****************************

.. automodule:: dds_ci.toolchain
  :members:


Module: ``dds_ci.testing``
**************************

.. automodule:: dds_ci.testing
  :members:


Module: ``dds_ci.testing.error``
*******************************

.. automodule:: dds_ci.testing.error
  :members:


Module: ``dds_ci.testing.fixtures``
***********************************

.. automodule:: dds_ci.testing.fixtures
  :members:


Module: ``dds_ci.testing.fs``
***********************************

.. automodule:: dds_ci.testing.fs
  :members:


Module: ``dds_ci.testing.http``
*******************************

.. automodule:: dds_ci.testing.http
  :members:


Module: ``dds_ci.testing.repo``
***********************************

.. automodule:: dds_ci.testing.repo
  :members:


External Types to Know
**********************

These types are defined externally and arg used extensively throughout the CI
scripts.

.. class:: _pytest.fixtures.FixtureRequest

  .. seealso:: :class:`pytest.FixtureRequest`

.. class:: _pytest.tmpdir.TempPathFactory

  .. seealso:: :class:`pytest.TempPathFactory`

.. class:: _pytest.config.Config

  .. seealso:: :class:`pytest.Config`
