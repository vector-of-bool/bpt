import multiprocessing
import shutil
import os
from pathlib import Path
import copy
from typing import Optional, Sequence, TypeVar, Iterable

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
        self.crs_cache_dir: Optional[Path] = None

    def clone(self: T) -> T:
        return copy.deepcopy(self)

    @property
    def pkg_db_path_arg(self) -> str:
        """The arguments for --catalog"""
        return f'--pkg-db-path={self.pkg_db_path}'

    @property
    def cache_dir_arg(self) -> Sequence[str]:
        """The arguments for --repo-dir"""
        default = [f'--pkg-cache-dir={self.repo_dir}']
        if self.crs_cache_dir:
            return [f'--crs-cache-dir={self.crs_cache_dir}'] + default
        else:
            return default

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

    def run(self, args: proc.CommandLine, *, cwd: Optional[Pathish] = None, timeout: Optional[float] = None) -> None:
        """Execute the 'dds' executable with the given arguments"""
        env = os.environ.copy()
        env['DDS_NO_ADD_INITIAL_REPO'] = '1'
        proc.check_run([self.path, args], cwd=cwd or self.default_cwd, env=env, timeout=timeout)

    def pkg_get(self, what: str) -> None:
        self.run(['pkg', 'get', self.pkg_db_path_arg, what])

    def repo_add(self, url: str) -> None:
        self.run(['pkg', 'repo', 'add', self.pkg_db_path_arg, url])

    def repo_remove(self, name: str) -> None:
        self.run(['pkg', 'repo', 'remove', self.pkg_db_path_arg, name])

    def repo_import(self, sdist: Path) -> None:
        self.run(['repo', self.cache_dir_arg, 'import', sdist])

    def pkg_import(self, filepath: Pathish) -> None:
        self.run(['pkg', 'import', filepath, self.cache_dir_arg])

    def pkg_prefetch(self, *, repos: Sequence[str], pkgs: Sequence[str] = ()) -> None:
        self.run([self.cache_dir_arg, 'pkg', 'prefetch', (f'--use-repo={r}' for r in repos), pkgs])

    def build(self,
              *,
              root: Pathish,
              toolchain: Optional[Pathish] = None,
              build_root: Optional[Pathish] = None,
              jobs: Optional[int] = None,
              tweaks_dir: Optional[Pathish] = None,
              with_tests: bool = True,
              more_args: Optional[proc.CommandLine] = None,
              timeout: Optional[float] = None) -> None:
        """
        Run 'dds build' with the given arguments.

        :param toolchain: The toolchain to use for the build.
        :param root: The root project directory.
        :param build_root: The root directory where the output will be written.
        :param with_tests: Include tests as part of the build
        :param jobs: The number of jobs to use. Default is CPU-count + 2
        """
        toolchain = toolchain or tc_mod.get_default_audit_toolchain()
        jobs = jobs or multiprocessing.cpu_count() + 2
        self.run(
            [
                'build',
                f'--toolchain={toolchain}',
                self.cache_dir_arg,
                self.pkg_db_path_arg,
                f'--jobs={jobs}',
                f'--project={root}',
                f'--out={build_root}',
                () if with_tests else ('--no-tests'),
                f'--tweaks-dir={tweaks_dir}' if tweaks_dir else (),
                more_args or (),
            ],
            timeout=timeout,
        )

    def compile_file(self,
                     paths: Iterable[Pathish],
                     *,
                     tweaks_dir: Optional[Pathish] = None,
                     toolchain: Optional[Pathish] = None,
                     project_dir: Pathish,
                     out: Optional[Pathish] = None) -> None:
        """
        Run 'dds compile-file' for the given paths.
        """
        toolchain = toolchain or tc_mod.get_default_audit_toolchain()
        self.run([
            'compile-file',
            self.pkg_db_path_arg,
            self.cache_dir_arg,
            paths,
            f'--tweaks-dir={tweaks_dir}' if tweaks_dir else (),
            f'--toolchain={toolchain}',
            f'--project={project_dir}',
            f'--out={out}',
        ])

    def build_deps(self, args: proc.CommandLine, *, toolchain: Optional[Path] = None) -> None:
        toolchain = toolchain or tc_mod.get_default_audit_toolchain()
        self.run([
            'build-deps',
            f'--toolchain={toolchain}',
            self.pkg_db_path_arg,
            self.cache_dir_arg,
            args,
        ])
