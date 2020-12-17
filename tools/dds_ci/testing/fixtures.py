"""
Test fixtures used by DDS in pytest
"""

from pathlib import Path
import pytest
import json
import shutil
from typing import Sequence, cast, Optional
from typing_extensions import TypedDict

from _pytest.config import Config as PyTestConfig
from _pytest.tmpdir import TempPathFactory
from _pytest.fixtures import FixtureRequest

from dds_ci import toolchain, paths
from ..dds import DDSWrapper, NewDDSWrapper
from ..util import Pathish
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


class _PackageJSONRequired(TypedDict):
    name: str
    namespace: str
    version: str


class PackageJSON(_PackageJSONRequired, total=False):
    depends: Sequence[str]


class _LibraryJSONRequired(TypedDict):
    name: str


class LibraryJSON(_LibraryJSONRequired, total=False):
    uses: Sequence[str]


class Project:
    def __init__(self, dirpath: Path, dds: DDSWrapper) -> None:
        self.dds = dds
        self.root = dirpath
        self.build_root = dirpath / '_build'

    @property
    def package_json(self) -> PackageJSON:
        return cast(PackageJSON, json.loads(self.root.joinpath('package.jsonc').read_text()))

    @package_json.setter
    def package_json(self, data: PackageJSON) -> None:
        self.root.joinpath('package.jsonc').write_text(json.dumps(data, indent=2))

    @property
    def library_json(self) -> LibraryJSON:
        return cast(LibraryJSON, json.loads(self.root.joinpath('library.jsonc').read_text()))

    @library_json.setter
    def library_json(self, data: LibraryJSON) -> None:
        self.root.joinpath('library.jsonc').write_text(json.dumps(data, indent=2))

    @property
    def project_dir_arg(self) -> str:
        """Argument for --project"""
        return f'--project={self.root}'

    def build(self, *, toolchain: Optional[Pathish] = None) -> None:
        """
        Execute 'dds build' on the project
        """
        with tc_mod.fixup_toolchain(toolchain or tc_mod.get_default_test_toolchain()) as tc:
            self.dds.build(root=self.root, build_root=self.build_root, toolchain=tc)

    def compile_file(self, *paths: Pathish, toolchain: Optional[Pathish] = None) -> None:
        with tc_mod.fixup_toolchain(toolchain or tc_mod.get_default_test_toolchain()) as tc:
            self.dds.compile_file(paths, toolchain=tc, out=self.build_root, project_dir=self.root)

    def sdist_create(self, *, dest: Optional[Pathish] = None) -> None:
        self.build_root.mkdir(exist_ok=True, parents=True)
        self.dds.run([
            'sdist',
            'create',
            self.project_dir_arg,
            f'--out={dest}' if dest else (),
        ], cwd=self.build_root)

    def sdist_export(self) -> None:
        self.dds.run(['sdist', 'export', self.dds.repo_dir_arg, self.project_dir_arg])

    def write(self, path: Pathish, content: str) -> Path:
        path = Path(path)
        if not path.is_absolute():
            path = self.root / path
        path.parent.mkdir(exist_ok=True, parents=True)
        path.write_text(content)
        return path


@pytest.fixture()
def test_parent_dir(request: FixtureRequest) -> Path:
    return Path(request.fspath).parent


class ProjectOpener():
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
        dirpath = Path(dirpath)
        if not dirpath.is_absolute():
            dirpath = self.test_dir / dirpath

        proj_copy = self.test_dir / '__test_project'
        if self._worker_id != 'master':
            proj_copy = self._tmppath_fac.mktemp('test-project-') / self.test_name
        else:
            self._request.addfinalizer(lambda: ensure_absent(proj_copy))

        shutil.copytree(dirpath, proj_copy)
        new_dds = self.dds.clone()

        if self._worker_id == 'master':
            repo_dir = self.test_dir / '__test_repo'
        else:
            repo_dir = self._tmppath_fac.mktemp('test-repo-') / self.test_name

        new_dds.set_repo_scratch(repo_dir)
        new_dds.default_cwd = proj_copy
        self._request.addfinalizer(lambda: ensure_absent(repo_dir))

        return Project(proj_copy, new_dds)


@pytest.fixture()
def project_opener(request: FixtureRequest, worker_id: str, dds_2: DDSWrapper,
                   tmp_path_factory: TempPathFactory) -> ProjectOpener:
    opener = ProjectOpener(dds_2, request, worker_id, tmp_path_factory)
    return opener


@pytest.fixture()
def tmp_project(request: FixtureRequest, worker_id: str, project_opener: ProjectOpener,
                tmp_path_factory: TempPathFactory) -> Project:
    if worker_id != 'master':
        proj_dir = tmp_path_factory.mktemp('temp-project')
        return project_opener.open(proj_dir)

    proj_dir = project_opener.test_dir / '__test_project_empty'
    ensure_absent(proj_dir)
    proj_dir.mkdir()
    proj = project_opener.open(proj_dir)
    request.addfinalizer(lambda: ensure_absent(proj_dir))
    return proj


@pytest.fixture(scope='session')
def dds_2(dds_exe: Path) -> NewDDSWrapper:
    wr = NewDDSWrapper(dds_exe)
    return wr


@pytest.fixture(scope='session')
def dds_exe(pytestconfig: PyTestConfig) -> Path:
    opt = pytestconfig.getoption('--dds-exe') or paths.BUILD_DIR / 'dds'
    return Path(opt)
