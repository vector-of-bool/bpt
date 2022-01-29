from pathlib import Path
import time
from contextlib import contextmanager
from typing import Callable, ContextManager, Dict, Iterator, NamedTuple, Optional, Union
import pytest
import hashlib
import base64
import json
import shutil

from dds_ci.testing.fixtures import TempPathFactory

from ..util import Pathish
from ..paths import PROJECT_ROOT


@pytest.fixture(scope='session')
def fs_render_cache_dir() -> Path:
    return PROJECT_ROOT / '_build/_test/dircache'


TreeData = Dict[str, Union[str, bytes, 'TreeData']]
_B64TreeData = Dict[str, Union[str, '_B64TreeData']]


def _b64_encode_tree(tree: TreeData) -> _B64TreeData:
    r: _B64TreeData = {}
    for key, val in tree.items():
        if isinstance(val, str):
            val = val.encode("utf-8")
        if isinstance(val, bytes):
            r[key] = base64.b64encode(val).decode('utf-8')
        else:
            r[key] = _b64_encode_tree(val)
    return r


class GetResult(NamedTuple):
    ready_path: Optional[Path]
    prep_path: Optional[Path]

    final_dest: Path

    def commit(self) -> Path:
        assert self.prep_path
        return self.prep_path.rename(self.final_dest)


def render_into(root: Pathish, tree: TreeData) -> Path:
    root = Path(root)
    root.mkdir(exist_ok=True, parents=True)
    for key, val in tree.items():
        child = root / key
        if isinstance(val, str):
            child.write_text(val, encoding='utf-8')
        elif isinstance(val, bytes):
            child.write_bytes(val)
        else:
            render_into(child, val)
    return root


class DirRenderer:
    def __init__(self, cache_root: Pathish, tmp_root: Pathish, tmp_factory: TempPathFactory) -> None:
        self._cache_path = Path(cache_root)
        self._tmp_path = Path(tmp_root)
        self._tmp_factory = tmp_factory

    def get_or_prepare(self, key: str) -> ContextManager[GetResult]:
        return self._get_or_prepare(key)

    @contextmanager
    def _get_or_prepare(self, key: str) -> Iterator[GetResult]:
        cached_path = self._cache_path / key
        if cached_path.is_dir():
            yield GetResult(cached_path, None, cached_path)
            return

        prep_path = cached_path.with_suffix(cached_path.suffix + '.tmp')
        try:
            prep_path.mkdir(parents=True)
        except FileExistsError:
            later = time.time() + 10
            while prep_path.exists() and time.time() < later:
                pass
            assert cached_path.is_dir()
            yield GetResult(cached_path, None, cached_path)
            return

        yield GetResult(None, prep_path, cached_path)
        shutil.rmtree(prep_path, ignore_errors=True)

    def get_or_render(self, name: str, tree: TreeData) -> Path:
        b64_tree = _b64_encode_tree(tree)
        hash = hashlib.md5(json.dumps(b64_tree, sort_keys=True).encode('utf-8')).hexdigest()[:4]
        key = f'{name}-{hash}'
        clone_dest = self._tmp_path / key
        shutil.rmtree(clone_dest, ignore_errors=True)
        with self.get_or_prepare(key) as prep:
            if prep.ready_path:
                shutil.copytree(prep.ready_path, clone_dest)
                return clone_dest
            assert prep.prep_path
            render_into(prep.prep_path, tree)
            return prep.commit()


@pytest.fixture(scope='session')
def dir_renderer(fs_render_cache_dir: Path, tmp_path_factory: TempPathFactory) -> DirRenderer:
    tp: Path = tmp_path_factory.mktemp('render-clones')
    return DirRenderer(fs_render_cache_dir, tp, tmp_path_factory)


TempCloner = Callable[[str, Pathish], Path]


@pytest.fixture(scope='session')
def tmp_clone_dir(tmp_path_factory: TempPathFactory) -> TempCloner:
    def _dup(name: str, p: Pathish) -> Path:
        tdir: Path = tmp_path_factory.mktemp(name) / '_'
        shutil.copytree(p, tdir)
        return tdir

    return _dup
