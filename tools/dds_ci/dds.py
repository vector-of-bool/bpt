import multiprocessing
import shutil
from pathlib import Path
import copy
from typing import Optional, TypeVar, Iterable

from . import paths, proc, toolchain as tc_mod
from dds_ci.util import Pathish

T = TypeVar('T')


class DDSWrapper:
    """
    Wraps a 'dds' executable with some convenience APIs that invoke various
    'dds' subcommands.
    """
    def __init__(self,
                 path: Path,
                 *,
                 repo_dir: Optional[Pathish] = None,
                 pkg_db_path: Optional[Pathish] = None,
                 default_cwd: Optional[Pathish] = None) -> None:
        self.path = path
        self.repo_dir = Path(repo_dir or (paths.PREBUILT_DIR / 'ci-repo'))
        self.pkg_db_path = Path(pkg_db_path or (self.repo_dir.parent / 'ci-catalog.db'))
        self.default_cwd = default_cwd or Path.cwd()

    def clone(self: T) -> T:
        return copy.deepcopy(self)

    @property
    def catalog_path_arg(self) -> str:
        """The arguments for --catalog"""
        return f'--catalog={self.pkg_db_path}'

    @property
    def repo_dir_arg(self) -> str:
        """The arguments for --repo-dir"""
        return f'--repo-dir={self.repo_dir}'

    @property
    def project_dir_flag(self) -> str:
        return '--project-dir'

    def set_repo_scratch(self, path: Pathish) -> None:
        self.repo_dir = Path(path) / 'data'
        self.pkg_db_path = Path(path) / 'pkgs.db'

    def clean(self, *, build_dir: Optional[Path] = None, repo: bool = True, pkg_db: bool = True) -> None:
        """
        Clean out prior executable output, including repos, pkg_db, and
        the build results at 'build_dir', if given.
        """
        if build_dir and build_dir.exists():
            shutil.rmtree(build_dir)
        if repo and self.repo_dir.exists():
            shutil.rmtree(self.repo_dir)
        if pkg_db and self.pkg_db_path.exists():
            self.pkg_db_path.unlink()

    def run(self, args: proc.CommandLine, *, cwd: Optional[Pathish] = None) -> None:
        """Execute the 'dds' executable with the given arguments"""
        proc.check_run([self.path, args], cwd=cwd or self.default_cwd)

    def catalog_json_import(self, path: Path) -> None:
        """Run 'catalog import' to import the given JSON. Only applicable to older 'dds'"""
        self.run(['catalog', 'import', self.catalog_path_arg, f'--json={path}'])

    def catalog_get(self, what: str) -> None:
        self.run(['catalog', 'get', self.catalog_path_arg, what])

    def pkg_get(self, what: str) -> None:
        self.run(['pkg', 'get', self.catalog_path_arg, what])

    def repo_add(self, url: str) -> None:
        self.run(['pkg', 'repo', 'add', self.catalog_path_arg, url])

    def repo_import(self, sdist: Path) -> None:
        self.run(['repo', self.repo_dir_arg, 'import', sdist])

    def pkg_import(self, filepath: Pathish) -> None:
        self.run(['pkg', 'import', filepath, self.repo_dir_arg])

    def build(self,
              *,
              root: Path,
              toolchain: Optional[Path] = None,
              build_root: Optional[Path] = None,
              jobs: Optional[int] = None) -> None:
        """
        Run 'dds build' with the given arguments.

        :param toolchain: The toolchain to use for the build.
        :param root: The root project directory.
        :param build_root: The root directory where the output will be written.
        :param jobs: The number of jobs to use. Default is CPU-count + 2
        """
        toolchain = toolchain or tc_mod.get_default_audit_toolchain()
        jobs = jobs or multiprocessing.cpu_count() + 2
        self.run([
            'build',
            f'--toolchain={toolchain}',
            self.repo_dir_arg,
            self.catalog_path_arg,
            f'--jobs={jobs}',
            f'{self.project_dir_flag}={root}',
            f'--out={build_root}',
        ])

    def compile_file(self,
                     paths: Iterable[Pathish],
                     *,
                     toolchain: Optional[Pathish] = None,
                     project_dir: Pathish,
                     out: Optional[Pathish] = None) -> None:
        """
        Run 'dds compile-file' for the given paths.
        """
        toolchain = toolchain or tc_mod.get_default_audit_toolchain()
        self.run([
            'compile-file',
            paths,
            f'--toolchain={toolchain}',
            f'{self.project_dir_flag}={project_dir}',
            f'--out={out}',
        ])

    def build_deps(self, args: proc.CommandLine, *, toolchain: Optional[Path] = None) -> None:
        toolchain = toolchain or tc_mod.get_default_audit_toolchain()
        self.run([
            'build-deps',
            f'--toolchain={toolchain}',
            self.catalog_path_arg,
            self.repo_dir_arg,
            args,
        ])


class NewDDSWrapper(DDSWrapper):
    """
    Wraps the new 'dds' executable with some convenience APIs
    """
    @property
    def repo_dir_arg(self) -> str:
        return f'--pkg-cache-dir={self.repo_dir}'

    @property
    def catalog_path_arg(self) -> str:
        return f'--pkg-db-path={self.pkg_db_path}'

    @property
    def project_dir_flag(self) -> str:
        return '--project'
