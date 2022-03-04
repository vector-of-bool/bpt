"""
Test fixtures used by DDS in pytest
"""

from __future__ import annotations
from copy import deepcopy

import json
import shutil
from contextlib import ExitStack
from pathlib import Path
from typing import (Any, Callable, Iterable, Iterator, KeysView, Mapping, Optional, Sequence, Union, cast)

import pytest
from _pytest.config import Config as PyTestConfig
from pytest import FixtureRequest, TempPathFactory
from typing_extensions import Literal, TypedDict

from .. import paths, toolchain
from ..dds import DDSWrapper
from ..proc import check_run
from ..util import JSONishArray, JSONishDict, JSONishValue, Pathish

tc_mod = toolchain


def ensure_absent(path: Pathish) -> None:
    path = Path(path)
    if path.is_dir():
        shutil.rmtree(path)
    elif path.exists():
        path.unlink()
    else:
        # File does not exist, wo we are safe to ignore it
        pass


_PkgYAMLLibraryUsingItem = TypedDict('_PkgYAMLLibraryUsingItem', {
    'lib': str,
    'for': Literal['lib', 'app', 'test'],
})


class _PkgYAMLDependencyItemRequired(TypedDict):
    dep: str


class _VersionItem(TypedDict):
    low: str
    high: str


_PkgYAMLDependencyItemOpt = TypedDict(
    '_ProjectJSONDependencyItemOpt',
    {
        'versions': Sequence[_VersionItem],
        'for': Literal['lib', 'app', 'test'],
        'using': Sequence[str],
    },
    total=False,
)


class _PkgYAMLDependencyItemMap(_PkgYAMLDependencyItemRequired, _PkgYAMLDependencyItemOpt):
    pass


_PkgYAMLDependencyItem = Union[str, _PkgYAMLDependencyItemMap]


class _PkgYAMLLibraryItemRequired(TypedDict):
    name: str
    path: str


class _PkgYAMLLibraryItem(_PkgYAMLLibraryItemRequired, total=False):
    using: Sequence[Union[str, _PkgYAMLLibraryUsingItem]]
    dependencies: Sequence[_PkgYAMLDependencyItem]


class _MainProjectLibraryItem(TypedDict, total=False):
    name: str
    dependencies: Sequence[_PkgYAMLDependencyItem]
    using: Sequence[Union[str, _PkgYAMLLibraryUsingItem]]


class _PkgYAMLRequired(TypedDict):
    name: str
    version: str


class PkgYAML(_PkgYAMLRequired, total=False):
    dependencies: Sequence[_PkgYAMLDependencyItem]
    lib: _MainProjectLibraryItem
    libs: Sequence[_PkgYAMLLibraryItem]


class Library:
    """
    Utilities to access a library under libs/ for a Project.
    """

    def __init__(self, name: str, dirpath: Path) -> None:
        self.name = name
        self.root = dirpath

    def write(self, path: Pathish, content: str) -> Path:
        """
        Write the given `content` to `path`. If `path` is relative, it will
        be resolved relative to the root directory of this library.
        """
        path = Path(path)
        if not path.is_absolute():
            path = self.root / path
        path.parent.mkdir(exist_ok=True, parents=True)
        path.write_text(content, encoding='utf-8')
        return path


class _WritebackData:

    def __init__(self, fpath: Path, items: JSONishDict | JSONishArray, root: JSONishDict) -> None:
        self._root = root
        self._data = items
        self._fpath = fpath

    def __setitem__(self, k: str | int, v: JSONishValue) -> None:
        if isinstance(self._data, Sequence):
            assert isinstance(k, int)
            self._data[k] = v
        else:
            assert isinstance(k, str)
            self._data[k] = v
        self._writeback()

    def __getitem__(self, k: str | int) -> JSONishValue:
        if isinstance(self._data, Sequence):
            assert isinstance(k, int)
            v = self._data[k]
        else:
            assert isinstance(k, str)
            v = self._data[k]
        return self._wrap_if_mutable(v)

    def __contains__(self, v: Any) -> bool:
        return v in self._data

    def _wrap_if_mutable(self, v: JSONishValue) -> JSONishValue:
        if isinstance(v, (Sequence, Mapping)) and not isinstance(v, str):
            return cast(JSONishValue, _WritebackData(self._fpath, v, self._root))
        return v

    def keys(self) -> KeysView[str]:
        assert isinstance(self._data, Mapping)
        return self._data.keys()

    def values(self) -> Iterable[JSONishValue]:
        assert isinstance(self._data, Mapping)
        return (self._wrap_if_mutable(v) for v in self._data.values())

    def items(self) -> Iterable[tuple[str, JSONishValue]]:
        assert isinstance(self._data, Mapping)
        for key, val in self._data.items():
            yield key, self._wrap_if_mutable(val)

    def append(self, value: JSONishValue) -> None:
        assert isinstance(self._data, Sequence)
        self._data.append(deepcopy(value))
        self._writeback()

    __none = object()

    def pop(self, index: str | int | None = None, default: JSONishValue | object = __none) -> JSONishValue:
        d = self._data
        if isinstance(d, Mapping):
            assert isinstance(index, str)
            if default is not self.__none:
                v = d.pop(index)
            else:
                v = cast(JSONishValue, d.pop(index, default))
        elif index is None:
            v = d.pop()
        else:
            assert isinstance(index, int)
            v = d.pop(index)
        self._writeback()
        return v

    def __delitem__(self, key: str) -> None:
        self.pop(key)

    def __iter__(self) -> Iterator[JSONishValue]:
        return iter(self._data)

    def __len__(self) -> int:
        return len(self._data)

    def get(self, key: Any, default: Any = None) -> Any:
        if key in self._data:
            v = self._data[key]
            if isinstance(v, (Sequence, Mapping)) and not isinstance(v, str):
                return _WritebackData(self._fpath, v, self._root)
            return v
        return default

    def insert(self, index: int, value: JSONishValue) -> None:
        assert isinstance(self._data, Sequence)
        self._data.insert(index, deepcopy(value))
        self._writeback()

    def _writeback(self) -> None:
        self._fpath.write_text(json.dumps(self._data, indent=2))


_WritebackData(Path(''), {}, {})


class Project:
    """
    Utilities to access a project being used as a test.
    """

    def __init__(self, dirpath: Path, dds: DDSWrapper) -> None:
        self.dds = dds.clone()
        self.root = dirpath
        self.build_root = dirpath / '_build'

    @property
    def pkg_yaml(self) -> PkgYAML:
        """
        Get/set the content of the `pkg.yaml` file for the project.
        """
        dat = json.loads(self.root.joinpath('pkg.yaml').read_text())
        return cast(PkgYAML, _WritebackData(self.root.joinpath('pkg.yaml'), dat, dat))

    @pkg_yaml.setter
    def pkg_yaml(self, data: PkgYAML) -> None:
        self.root.joinpath('pkg.yaml').write_text(json.dumps(data, indent=2))

    def lib(self, name: str) -> Library:
        return Library(name, self.root / f'libs/{name}')

    @property
    def __root_library(self) -> Library:
        """
        The root/default library for this project
        """
        return Library('<default>', self.root)

    @property
    def project_dir_arg(self) -> str:
        """Argument for --project"""
        return f'--project={self.root}'

    def build(self,
              *,
              toolchain: Optional[Pathish] = None,
              fixup_toolchain: bool = True,
              jobs: Optional[int] = None,
              timeout: Union[float, None] = None,
              tweaks_dir: Optional[Path] = None,
              with_tests: bool = True,
              repos: Sequence[Pathish] = (),
              log_level: Literal['info', 'debug', 'trace'] = 'trace') -> None:
        """
        Execute 'dds build' on the project
        """
        with ExitStack() as scope:
            if fixup_toolchain:
                toolchain = scope.enter_context(tc_mod.fixup_toolchain(toolchain
                                                                       or tc_mod.get_default_test_toolchain()))
            self.dds.build(root=self.root,
                           build_root=self.build_root,
                           toolchain=toolchain,
                           jobs=jobs,
                           timeout=timeout,
                           tweaks_dir=tweaks_dir,
                           with_tests=with_tests,
                           repos=repos,
                           more_args=[f'--log-level={log_level}'])

    def compile_file(self, *paths: Pathish, toolchain: Optional[Pathish] = None) -> None:
        with tc_mod.fixup_toolchain(toolchain or tc_mod.get_default_test_toolchain()) as tc:
            self.dds.compile_file(paths, toolchain=tc, out=self.build_root, project_dir=self.root)

    def pkg_create(self, *, dest: Optional[Pathish] = None, if_exists: Optional[str] = None) -> None:
        self.build_root.mkdir(exist_ok=True, parents=True)
        self.dds.run(
            [
                'pkg',
                'create',
                self.project_dir_arg,
                f'--out={dest}' if dest else (),
                f'--if-exists={if_exists}' if if_exists else (),
            ],
            cwd=self.build_root,
        )

    def write(self, path: Pathish, content: str) -> Path:
        """
        Write the given `content` to `path`. If `path` is relative, it will
        be resolved relative to the root directory of this project.
        """
        return self.__root_library.write(path, content)


@pytest.fixture(scope='module')
def test_parent_dir(request: FixtureRequest) -> Path:
    """
    :class:`pathlib.Path` fixture pointing to the parent directory of the file
    containing the test that is requesting the current fixture
    """
    return Path(request.fspath).parent


class ProjectOpener():
    """
    A test fixture that opens project directories for testing
    """

    def __init__(self, dds: DDSWrapper, request: FixtureRequest, worker: str,
                 tmp_path_factory: TempPathFactory) -> None:
        self.dds = dds
        self._request = request
        self._worker_id = worker
        self._tmppath_fac = tmp_path_factory

    @property
    def test_name(self) -> str:
        """The name of the test that requested this opener"""
        func: Any = self._request.function  # type: ignore
        return func.__name__  # type: ignore

    @property
    def test_dir(self) -> Path:
        """The directory that contains the test that requested this opener"""
        return Path(self._request.fspath).parent

    def open(self, dirpath: Pathish) -> Project:
        """
        Open a new project testing fixture from the given project directory.

        :param dirpath: The directory that contains the project to use.

        Clones the given directory and then opens a project within that clone.
        The clone directory will be destroyed when the test fixture is torn down.
        """
        dirpath = Path(dirpath)
        if not dirpath.is_absolute():
            dirpath = self.test_dir / dirpath

        proj_copy = self.test_dir / '__test_project'
        if self._worker_id != 'master':
            proj_copy: Path = self._tmppath_fac.mktemp('test-project-') / self.test_name
        else:
            self._request.addfinalizer(lambda: ensure_absent(proj_copy))

        shutil.copytree(dirpath, proj_copy)
        new_dds = self.dds.clone()

        if self._worker_id == 'master':
            crs_dir = self.test_dir / '__test_crs'
        else:
            crs_dir: Path = self._tmppath_fac.mktemp('test-crs-') / self.test_name

        new_dds.crs_cache_dir = crs_dir
        new_dds.default_cwd = proj_copy
        self._request.addfinalizer(lambda: ensure_absent(crs_dir))

        return Project(proj_copy, new_dds)


@pytest.fixture()
def project_opener(request: FixtureRequest, worker_id: str, dds: DDSWrapper,
                   tmp_path_factory: TempPathFactory) -> ProjectOpener:
    """
    A fixture factory that can open directories as Project objects for building
    and testing. Duplicates the project directory into a temporary location so
    that the original test directory remains unchanged.
    """
    opener = ProjectOpener(dds, request, worker_id, tmp_path_factory)
    return opener


@pytest.fixture()
def tmp_project(request: FixtureRequest, worker_id: str, project_opener: ProjectOpener,
                tmp_path_factory: TempPathFactory) -> Project:
    """
    A fixture that generates an empty temporary project directory that will be thrown away
    when the test completes.
    """
    if worker_id != 'master':
        proj_dir: Path = tmp_path_factory.mktemp('temp-project')
        return project_opener.open(proj_dir)

    proj_dir = project_opener.test_dir / '__test_project_empty'
    ensure_absent(proj_dir)
    proj_dir.mkdir()
    proj = project_opener.open(proj_dir)
    request.addfinalizer(lambda: ensure_absent(proj_dir))
    return proj


@pytest.fixture(scope='session')
def dds(dds_exe: Path) -> DDSWrapper:
    """
    A :class:`~dds_ci.dds.DDSWrapper` around the dds executable under test
    """
    wr = DDSWrapper(dds_exe)
    return wr


@pytest.fixture(scope='session')
def dds_exe(pytestconfig: PyTestConfig) -> Path:
    """A :class:`pathlib.Path` pointing to the DDS executable under test"""
    opt = pytestconfig.getoption('--dds-exe') or paths.BUILD_DIR / 'for-test/dds'
    assert isinstance(opt, (Path, str))
    return Path(opt)


TmpGitRepoFactory = Callable[[Pathish], Path]


@pytest.fixture(scope='session')
def tmp_git_repo_factory(tmp_path_factory: TempPathFactory, pytestconfig: PyTestConfig) -> TmpGitRepoFactory:
    """
    A temporary directory :class:`pathlib.Path` object in which a git repo will
    be initialized
    """

    def f(dirpath: Pathish) -> Path:
        test_dir = Path()
        dirpath = Path(dirpath)
        if not dirpath.is_absolute():
            dirpath = test_dir / dirpath

        tmp_path: Path = tmp_path_factory.mktemp('tmp-git')

        # Could use dirs_exists_ok=True with Python 3.8, but we min dep on 3.6
        repo = tmp_path / 'r'
        shutil.copytree(dirpath, repo)

        git = pytestconfig.getoption('--git-exe') or 'git'
        assert isinstance(git, str)
        check_run([git, 'init', repo])
        check_run([git, 'checkout', '-b', 'tmp_git_repo'], cwd=repo)
        check_run([git, 'add', '-A'], cwd=repo)
        check_run(
            [git, '-c', "user.name='Tmp Git'", '-c', "user.email='dds@example.org'", 'commit', '-m', 'Initial commit'],
            cwd=repo)

        return repo

    return f
