"""
Test fixtures and utilities for creating and using CRS repositories
"""

from pathlib import Path
from typing import Callable, NamedTuple
from typing_extensions import Literal

import pytest
from _pytest.tmpdir import TempPathFactory
from _pytest.fixtures import FixtureRequest

from dds_ci.dds import DDSWrapper
from .http import HTTPServerFactory, ServerInfo


class CRSRepo:
    """
    A CRS repository directory
    """
    def __init__(self, path: Path, dds: DDSWrapper) -> None:
        self.path = path
        self.dds = dds

    def import_dir(self, path: Path, *, if_exists: Literal[None, 'replace', 'ignore', 'fail'] = None) -> None:
        self.dds.run([
            'repo',
            'import',
            self.path,
            path,
            (() if if_exists is None else f'--if-exists={if_exists}'),
        ])

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


@pytest.fixture()
def tmp_crs_repo(crs_repo_factory: CRSRepoFactory, request: FixtureRequest) -> CRSRepo:
    return crs_repo_factory(str(request.function.__name__))


class CRSRepoServer(NamedTuple):
    """
    An HTTP-server CRS repository
    """
    repo: CRSRepo
    server: ServerInfo


@pytest.fixture()
def http_crs_repo(tmp_crs_repo: CRSRepo, http_server_factory: HTTPServerFactory) -> CRSRepoServer:
    server = http_server_factory(tmp_crs_repo.path)
    return CRSRepoServer(tmp_crs_repo, server)
