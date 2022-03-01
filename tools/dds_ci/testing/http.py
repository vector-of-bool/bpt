import socket
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack, closing, contextmanager
from functools import partial
from http.server import HTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
from typing import Any, Callable, Iterator, NamedTuple, cast

import pytest
from pytest import FixtureRequest


def _unused_tcp_port() -> int:
    """Find an unused localhost TCP port from 1024-65535 and return it."""
    with closing(socket.socket()) as sock:
        sock.bind(('127.0.0.1', 0))
        return cast(int, sock.getsockname()[1])


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
