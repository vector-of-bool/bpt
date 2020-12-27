import pytest

from dds_ci.dds import DDSWrapper
from dds_ci.testing.fixtures import Project
from dds_ci.testing.http import RepoFixture
from dds_ci.testing.error import expect_error_marker
from pathlib import Path


@pytest.fixture()
def tmp_repo(tmp_path: Path, dds: DDSWrapper) -> Path:
    dds.run(['repoman', 'init', tmp_path])
    return tmp_path


def test_error_bad_pkg_id(dds: DDSWrapper, tmp_repo: Path) -> None:
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
    dds.run(['repoman', 'add', tmp_repo, 'neo-fun@0.6.0', 'github:vector-of-bool/neo-fun/0.6.0'])
    with expect_error_marker('dup-pkg-add'):
        dds.run(['repoman', 'add', tmp_repo, 'neo-fun@0.6.0', 'github:vector-of-bool/neo-fun/0.6.0'])


def test_add_invalid(dds: DDSWrapper, tmp_repo: Path) -> None:
    with expect_error_marker('repoman-add-invalid-pkg-url'):
        dds.run(['repoman', 'add', tmp_repo, 'foo@1.2.3', 'invalid://google.com/lolwut'])


def test_error_double_remove(tmp_repo: Path, dds: DDSWrapper) -> None:
    dds.run([
        'repoman', '-ltrace', 'add', tmp_repo, 'neo-fun@0.4.0',
        'https://github.com/vector-of-bool/neo-fun/archive/0.4.0.tar.gz?__dds_strpcmp=1'
    ])
    dds.run(['repoman', 'remove', tmp_repo, 'neo-fun@0.4.0'])

    with expect_error_marker('repoman-rm-no-such-package'):
        dds.run(['repoman', 'remove', tmp_repo, 'neo-fun@0.4.0'])


def test_pkg_http(http_repo: RepoFixture, tmp_project: Project) -> None:
    tmp_project.dds.run([
        'repoman', '-ltrace', 'add', http_repo.server.root, 'neo-fun@0.4.0',
        'https://github.com/vector-of-bool/neo-fun/archive/0.4.0.tar.gz?__dds_strpcmp=1'
    ])
    tmp_project.dds.repo_add(http_repo.url)
    tmp_project.package_json = {
        'name': 'test',
        'version': '1.2.3',
        'depends': ['neo-fun@0.4.0'],
        'namespace': 'test',
    }
    tmp_project.build()
