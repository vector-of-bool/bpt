"""
Test fixtures and utilities for creating and using CRS repositories
"""

from pathlib import Path
from typing import Any, Callable, Iterable, NamedTuple, Union

import pytest
from typing_extensions import Literal

from ..dds import DDSWrapper
from .fixtures import TempPathFactory
from .fs import TempCloner
from .http import HTTPServerFactory, ServerInfo


class CRSRepo:
    """
    A CRS repository directory
    """
    def __init__(self, path: Path, dds: DDSWrapper) -> None:
        self.path = path
        self.dds = dds

    def import_(self,
                path: Union[Path, Iterable[Path]],
                *,
                if_exists: Literal[None, 'replace', 'ignore', 'fail'] = None,
                validate: bool = True) -> None:
        if isinstance(path, Path):
            path = [path]
        self.dds.run([
            '-ldebug',
            'repo',
            'import',
            self.path,
            path,
            (() if if_exists is None else f'--if-exists={if_exists}'),
        ])
        if validate:
            self.validate()

    def validate(self) -> None:
        self.dds.run(['repo', 'validate', self.path, '-ldebug'])


CRSRepoFactory = Callable[[str], CRSRepo]


@pytest.fixture(scope='session')
def crs_repo_factory(tmp_path_factory: TempPathFactory, dds: DDSWrapper) -> CRSRepoFactory:
    def _make(name: str) -> CRSRepo:
        tmpdir = Path(tmp_path_factory.mktemp('crs-repo-'))
        dds.run(['repo', 'init', tmpdir, f'--name={name}'])
        return CRSRepo(tmpdir, dds)

    return _make


@pytest.fixture(scope='session')
def session_empty_crs_repo(crs_repo_factory: CRSRepoFactory) -> CRSRepo:
    return crs_repo_factory('session-empty')


RepoCloner = Callable[[CRSRepo], CRSRepo]


@pytest.fixture(scope='session')
def clone_repo(tmp_clone_dir: TempCloner) -> RepoCloner:
    def _clone(repo: CRSRepo) -> CRSRepo:
        clone = tmp_clone_dir('repo', repo.path)
        return CRSRepo(clone, repo.dds)

    return _clone


@pytest.fixture()
def tmp_crs_repo(session_empty_crs_repo: CRSRepo, clone_repo: RepoCloner) -> CRSRepo:
    return clone_repo(session_empty_crs_repo)


class CRSRepoServer(NamedTuple):
    """
    An HTTP-server CRS repository
    """
    repo: CRSRepo
    server: ServerInfo


@pytest.fixture()
def http_crs_repo(tmp_crs_repo: CRSRepo, http_server_factory: HTTPServerFactory) -> CRSRepoServer:
    """Generate a temporary HTTP server serving a temporary CRS repository"""
    server = http_server_factory(tmp_crs_repo.path)
    return CRSRepoServer(tmp_crs_repo, server)


def make_simple_crs(name: str, version: str, *, pkg_version: int = 1) -> Any:
    return {
        'schema-version': 1,
        'name': name,
        'version': version,
        'pkg-version': pkg_version,
        'libraries': [{
            'path': '.',
            'name': name,
            'using': [],
            'dependencies': [],
        }]
    }
