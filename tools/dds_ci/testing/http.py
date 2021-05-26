from pathlib import Path
import socket
from contextlib import contextmanager, ExitStack, closing
import json
from http.server import SimpleHTTPRequestHandler, HTTPServer
from typing import NamedTuple, Any, Iterator, Callable
from concurrent.futures import ThreadPoolExecutor
from functools import partial
import tempfile
import sys
import subprocess

import pytest
from _pytest.fixtures import FixtureRequest
from _pytest.tmpdir import TempPathFactory

from dds_ci.dds import DDSWrapper


def _unused_tcp_port() -> int:
    """Find an unused localhost TCP port from 1024-65535 and return it."""
    with closing(socket.socket()) as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


class DirectoryServingHTTPRequestHandler(SimpleHTTPRequestHandler):
    """
    A simple HTTP request handler that simply serves files from a directory given to the constructor.
    """
    def __init__(self, *args: Any, **kwargs: Any) -> None:
        self.dir = kwargs.pop('dir')
        super().__init__(*args, **kwargs)

    def translate_path(self, path: str) -> str:
        # Convert the given URL path to a path relative to the directory we are serving
        abspath = Path(super().translate_path(path))  # type: ignore
        relpath = abspath.relative_to(Path.cwd())
        return str(self.dir / relpath)


class ServerInfo(NamedTuple):
    """
    Information about an HTTP server fixture
    """
    base_url: str
    root: Path


@contextmanager
def run_http_server(dirpath: Path, port: int) -> Iterator[ServerInfo]:
    """
    Context manager that spawns an HTTP server that serves thegiven directory on
    the given TCP port.
    """
    handler = partial(DirectoryServingHTTPRequestHandler, dir=dirpath)
    addr = ('127.0.0.1', port)
    pool = ThreadPoolExecutor()
    with HTTPServer(addr, handler) as httpd:
        pool.submit(lambda: httpd.serve_forever(poll_interval=0.1))
        try:
            print('Serving at', addr)
            yield ServerInfo(f'http://127.0.0.1:{port}', dirpath)
        finally:
            httpd.shutdown()


HTTPServerFactory = Callable[[Path], ServerInfo]


@pytest.fixture(scope='session')
def http_server_factory(request: FixtureRequest) -> HTTPServerFactory:
    """
    Spawn an HTTP server that serves the content of a directory.
    """
    def _make(p: Path) -> ServerInfo:
        st = ExitStack()
        server = st.enter_context(run_http_server(p, _unused_tcp_port()))
        request.addfinalizer(st.pop_all)
        return server

    return _make


class RepoServer:
    """
    A fixture handle to a dds HTTP repository, including a path and URL.
    """
    def __init__(self, dds_exe: Path, info: ServerInfo, repo_name: str) -> None:
        self.repo_name = repo_name
        self.server = info
        self.url = info.base_url
        self.dds_exe = dds_exe

    def import_json_data(self, data: Any) -> None:
        """
        Import some packages into the repo for the given JSON data. Uses
        mkrepo.py
        """
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(json.dumps(data).encode())
            f.close()
            self.import_json_file(Path(f.name))
            Path(f.name).unlink()

    def import_json_file(self, fpath: Path) -> None:
        """
        Import some package into the repo for the given JSON file. Uses mkrepo.py
        """
        subprocess.check_call([
            sys.executable,
            str(Path.cwd() / 'tools/mkrepo.py'),
            f'--dds-exe={self.dds_exe}',
            f'--dir={self.server.root}',
            f'--spec={fpath}',
        ])


RepoFactory = Callable[[str], Path]


@pytest.fixture(scope='session')
def repo_factory(tmp_path_factory: TempPathFactory, dds: DDSWrapper) -> RepoFactory:
    def _make(name: str) -> Path:
        tmpdir = tmp_path_factory.mktemp('test-repo-')
        dds.run(['repoman', 'init', tmpdir, f'--name={name}'])
        return tmpdir

    return _make


HTTPRepoServerFactory = Callable[[str], RepoServer]


@pytest.fixture(scope='session')
def http_repo_factory(dds_exe: Path, repo_factory: RepoFactory,
                      http_server_factory: HTTPServerFactory) -> HTTPRepoServerFactory:
    """
    Fixture factory that creates new repositories with an HTTP server for them.
    """
    def _make(name: str) -> RepoServer:
        repo_dir = repo_factory(name)
        server = http_server_factory(repo_dir)
        return RepoServer(dds_exe, server, name)

    return _make


@pytest.fixture(scope='module')
def module_http_repo(http_repo_factory: HTTPRepoServerFactory, request: FixtureRequest) -> RepoServer:
    return http_repo_factory(f'module-repo-{request.module.__name__}')


@pytest.fixture()
def http_repo(http_repo_factory: HTTPRepoServerFactory, request: FixtureRequest) -> RepoServer:
    """
    Fixture that creates a new empty dds repository and an HTTP server to serve
    it.
    """
    return http_repo_factory(f'test-repo-{request.function.__name__}')
