from pathlib import Path
from contextlib import contextmanager
import json
from http.server import SimpleHTTPRequestHandler, HTTPServer
from typing import NamedTuple
from concurrent.futures import ThreadPoolExecutor
from functools import partial
import tempfile
import sys
import subprocess

import pytest


class DirectoryServingHTTPRequestHandler(SimpleHTTPRequestHandler):
    """
    A simple HTTP request handler that simply serves files from a directory given to the constructor.
    """
    def __init__(self, *args, **kwargs) -> None:
        self.dir = kwargs.pop('dir')
        super().__init__(*args, **kwargs)

    def translate_path(self, path) -> str:
        # Convert the given URL path to a path relative to the directory we are serving
        abspath = Path(super().translate_path(path))
        relpath = abspath.relative_to(Path.cwd())
        return str(self.dir / relpath)


class ServerInfo(NamedTuple):
    """
    Information about an HTTP server fixture
    """
    base_url: str
    root: Path


@contextmanager
def run_http_server(dirpath: Path, port: int):
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


@pytest.yield_fixture()
def http_tmp_dir_server(tmp_path: Path, unused_tcp_port: int):
    """
    Creates an HTTP server that serves the contents of a new
    temporary directory.
    """
    with run_http_server(tmp_path, unused_tcp_port) as s:
        yield s


class RepoFixture:
    """
    A fixture handle to a dds HTTP repository, including a path and URL.
    """
    def __init__(self, dds_exe: Path, info: ServerInfo) -> None:
        self.server = info
        self.url = info.base_url
        self.dds_exe = dds_exe

    def import_json_data(self, data) -> None:
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


@pytest.yield_fixture()
def http_repo(dds_exe: Path, http_tmp_dir_server: ServerInfo):
    """
    Fixture that creates a new empty dds repository and an HTTP server to serve
    it.
    """
    subprocess.check_call([str(dds_exe), 'repoman', 'init', str(http_tmp_dir_server.root)])
    yield RepoFixture(dds_exe, http_tmp_dir_server)
