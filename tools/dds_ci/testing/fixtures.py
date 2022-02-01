"""
Test fixtures used by DDS in pytest
"""

from contextlib import ExitStack
from pathlib import Path
import pytest
import json
import shutil
from typing import Sequence, Union, cast, Optional, Callable
from typing_extensions import Literal, TypedDict

from _pytest.config import Config as PyTestConfig
from pytest import FixtureRequest, TempPathFactory

from dds_ci import toolchain, paths
from ..dds import DDSWrapper
from ..util import Pathish
from ..proc import check_run
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


_PkgYAMLLibraryUsesItem = TypedDict('_PkgYAMLLibraryUsesItem', {
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
        'uses': Sequence[str],
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
    uses: Sequence[Union[str, _PkgYAMLLibraryUsesItem]]
    depends: Sequence[_PkgYAMLDependencyItem]


class _MainProjectLibraryItem(TypedDict, total=False):
    name: str
    depends: Sequence[_PkgYAMLDependencyItem]
    uses: Sequence[Union[str, _PkgYAMLLibraryUsesItem]]


class _PkgYAMLRequired(TypedDict):
    name: str
    version: str


class PkgYAML(_PkgYAMLRequired, total=False):
    depends: Sequence[_PkgYAMLDependencyItem]
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
        path.write_text(content)
        return path


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
        return cast(PkgYAML, json.loads(self.root.joinpath('pkg.yaml').read_text()))

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

    def sdist_export(self) -> None:
        self.dds.run(['sdist', 'export', self.dds.cache_dir_arg, self.project_dir_arg])

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
        return str(self._request.function.__name__)

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
    opt: Path = pytestconfig.getoption('--dds-exe') or paths.BUILD_DIR / 'dds'
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

        git: str = pytestconfig.getoption('--git-exe') or 'git'
        check_run([git, 'init', repo])
        check_run([git, 'checkout', '-b', 'tmp_git_repo'], cwd=repo)
        check_run([git, 'add', '-A'], cwd=repo)
        check_run(
            [git, '-c', "user.name='Tmp Git'", '-c', "user.email='dds@example.org'", 'commit', '-m', 'Initial commit'],
            cwd=repo)

        return repo

    return f
