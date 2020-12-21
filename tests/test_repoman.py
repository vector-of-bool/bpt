import pytest

from dds_ci import dds
from dds_ci.testing.fixtures import DDSWrapper, Project
from dds_ci.testing.error import expect_error_marker
from pathlib import Path


@pytest.fixture()
def tmp_repo(tmp_path: Path, dds: DDSWrapper) -> Path:
    dds.run(['repoman', 'init', tmp_path])
    return tmp_path


def test_bad_pkg_id(dds: DDSWrapper, tmp_repo: Path) -> None:
    with expect_error_marker('invalid-pkg-id-str-version'):
        dds.run(['repoman', 'add', tmp_repo, 'foo@bar', 'http://example.com'])

    with expect_error_marker('invalid-pkg-id-str'):
        dds.run(['repoman', 'add', tmp_repo, 'foo', 'http://example.com'])


def test_add_simple(dds: DDSWrapper, tmp_repo: Path) -> None:
    dds.run(['repoman', 'add', tmp_repo, 'neo-fun@0.6.0', 'git+https://github.com/vector-of-bool/neo-fun.git#0.6.0'])
    with expect_error_marker('dup-pkg-add'):
        dds.run(
            ['repoman', 'add', tmp_repo, 'neo-fun@0.6.0', 'git+https://github.com/vector-of-bool/neo-fun.git#0.6.0'])


def test_add_github(dds: DDSWrapper, tmp_repo: Path) -> None:
    dds.run(['repoman', 'add', tmp_repo, 'neo-fun@0.6.0', 'github:vector-of-bool/neo-fun#0.6.0'])
    with expect_error_marker('dup-pkg-add'):
        dds.run(['repoman', 'add', tmp_repo, 'neo-fun@0.6.0', 'github:vector-of-bool/neo-fun#0.6.0'])
