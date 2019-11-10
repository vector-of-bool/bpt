import pytest
import subprocess

from tests import DDS, DDSFixtureParams, dds_fixture_conf

dds_conf = dds_fixture_conf(
    DDSFixtureParams(ident='git-remote', subdir='git-remote'),
    DDSFixtureParams(ident='no-deps', subdir='no-deps'),
)


@dds_conf
def test_ls(dds: DDS):
    dds.run(['deps', 'ls'])


@dds_conf
def test_deps_build(dds: DDS):
    assert not dds.repo_dir.exists()
    dds.deps_get()
    assert dds.repo_dir.exists(), '`deps get` did not generate a repo directory'

    assert not dds.lmi_path.exists()
    dds.deps_build()
    assert dds.lmi_path.exists(), '`deps build` did not generate the build dir'
