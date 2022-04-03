BPT CI Scripts Python API
#########################

Test Fixtures
*************

The following test fixtures are defined:

- :func:`~bpt_ci.testing.fixtures.bpt` - :class:`bpt_ci.bpt.BPTWrapper` - A
  wrapper around the ``bpt`` executable under test.
- :func:`~bpt_ci.testing.fixtures.tmp_project` -
  :class:`bpt_ci.testing.fixtures.Project` - Create a new empty directory to be
  used as a test project for ``bpt`` to execute.
- :func:`~bpt_ci.testing.repo.http_crs_repo` -
  :class:`bpt_ci.testing.repo.CRSRepoServer` - Create a new bpt repository and
  spawn an HTTP server to serve it.


Module: ``bpt_ci``
******************

.. automodule:: bpt_ci
  :members:



Module: ``bpt_ci.bootstrap``
****************************

.. automodule:: bpt_ci.bootstrap
  :members:


Module: ``bpt_ci.bpt``
**********************

.. automodule:: bpt_ci.bpt
  :members:


Module: ``bpt_ci.docs``
***********************

.. automodule:: bpt_ci.docs
  :members:


Module: ``bpt_ci.msvs``
***********************

.. automodule:: bpt_ci.msvs
  :members:


Module: ``bpt_ci.paths``
************************

.. automodule:: bpt_ci.paths
  :members:


Module: ``bpt_ci.proc``
***********************

.. automodule:: bpt_ci.proc
  :members:


Module: ``bpt_ci.tasks``
************************

.. automodule:: bpt_ci.tasks
  :members:


Module: ``bpt_ci.util``
***********************

.. automodule:: bpt_ci.util
  :members:


Module: ``bpt_ci.toolchain``
****************************

.. automodule:: bpt_ci.toolchain
  :members:


Module: ``bpt_ci.testing``
**************************

.. automodule:: bpt_ci.testing
  :members:


Module: ``bpt_ci.testing.error``
*******************************

.. automodule:: bpt_ci.testing.error
  :members:


Module: ``bpt_ci.testing.fixtures``
***********************************

.. automodule:: bpt_ci.testing.fixtures
  :members:


Module: ``bpt_ci.testing.fs``
***********************************

.. automodule:: bpt_ci.testing.fs
  :members:


Module: ``bpt_ci.testing.http``
*******************************

.. automodule:: bpt_ci.testing.http
  :members:


Module: ``bpt_ci.testing.repo``
***********************************

.. automodule:: bpt_ci.testing.repo
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
