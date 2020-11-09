import json
from pathlib import Path
from functools import partial
from concurrent.futures import ThreadPoolExecutor
from http.server import SimpleHTTPRequestHandler, HTTPServer
import time

import pytest

from tests import dds, DDS
from tests.fileutil import ensure_dir


class DirectoryServingHTTPRequestHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs) -> None:
        self.dir = kwargs.pop('dir')
        super().__init__(*args, **kwargs)

    def translate_path(self, path) -> str:
        abspath = Path(super().translate_path(path))
        relpath = abspath.relative_to(Path.cwd())
        return self.dir / relpath


def test_import_json(dds: DDS):
    dds.scope.enter_context(ensure_dir(dds.build_dir))
    dds.catalog_create()

    json_fpath = dds.build_dir / 'data.json'
    import_data = {
        'version': 2,
        'packages': {
            'foo': {
                '1.2.4': {
                    'url': 'git+http://example.com#master',
                    'depends': [],
                },
                '1.2.5': {
                    'url': 'git+http://example.com#master',
                },
            },
            'bar': {
                '1.5.1': {
                    'url': 'http://example.com/bar-1.5.2.tgz'
                },
            }
        },
    }
    dds.scope.enter_context(
        dds.set_contents(json_fpath,
                         json.dumps(import_data).encode()))
    dds.catalog_import(json_fpath)


@pytest.yield_fixture
def http_import_server():
    handler = partial(
        DirectoryServingHTTPRequestHandler,
        dir=Path.cwd() / 'data/http-test-1')
    addr = ('0.0.0.0', 8000)
    pool = ThreadPoolExecutor()
    with HTTPServer(addr, handler) as httpd:
        pool.submit(lambda: httpd.serve_forever(poll_interval=0.1))
        try:
            yield
        finally:
            httpd.shutdown()


def test_import_http(dds: DDS, http_import_server):
    dds.repo_dir.mkdir(parents=True, exist_ok=True)
    dds.run(
        [
            'repo',
            dds.repo_dir_arg,
            'import',
            'https://github.com/vector-of-bool/neo-buffer/archive/0.4.2.tar.gz?dds_strpcmp=1',
        ],
        cwd=dds.repo_dir,
    )
    assert dds.repo_dir.joinpath('neo-buffer@0.4.2').is_dir()
