import copy
import multiprocessing
import os
import shutil
from pathlib import Path
from typing import Iterable, Optional, Sequence, TypeVar

from dds_ci.util import Pathish

from . import proc
from . import toolchain as tc_mod

T = TypeVar('T')


class DDSWrapper:
    """
    Wraps a 'dds' executable with some convenience APIs that invoke various
    'dds' subcommands.
    """

    def __init__(self,
                 path: Path,
                 *,
                 crs_cache_dir: Optional[Pathish] = None,
                 default_cwd: Optional[Pathish] = None) -> None:
        self.path = path
        self.default_cwd = Path(default_cwd or Path.cwd())
        self.crs_cache_dir = crs_cache_dir

    def clone(self: T) -> T:
        return copy.deepcopy(self)

    @property
    def always_args(self) -> proc.CommandLine:
        """Arguments that are always given to every dds subcommand"""
        return self.crs_cache_dir_arg

    @property
    def crs_cache_dir_arg(self) -> proc.CommandLine:
        """The arguments for --crs-cache-dir"""
        if self.crs_cache_dir is not None:
            return [f'--crs-cache-dir={self.crs_cache_dir}']
        return []

    def clean(self, *, build_dir: Optional[Path] = None, crs_cache: bool = True) -> None:
        """
        Clean out prior executable output, including repos, pkg_db, and
        the build results at 'build_dir', if given.
        """
        if build_dir and build_dir.exists():
            shutil.rmtree(build_dir)
        if crs_cache and self.crs_cache_dir:
            p = Path(self.crs_cache_dir)
            if p.exists():
                shutil.rmtree(p)

    def run(self, args: proc.CommandLine, *, cwd: Optional[Pathish] = None, timeout: Optional[float] = None) -> None:
        """Execute the 'dds' executable with the given arguments"""
        env = os.environ.copy()
        env['DDS_NO_ADD_INITIAL_REPO'] = '1'
        proc.check_run([self.path, self.always_args, args], cwd=cwd or self.default_cwd, env=env, timeout=timeout)

    def pkg_prefetch(self, *, repos: Sequence[Pathish], pkgs: Sequence[str] = ()) -> None:
        self.run(['pkg', 'prefetch', (f'--use-repo={r}' for r in repos), pkgs])

    def pkg_solve(self, *, repos: Sequence[Pathish], pkgs: Sequence[str]) -> None:
        self.run(['pkg', 'solve', (f'--use-repo={r}' for r in repos), pkgs])

    def build(self,
              *,
              root: Pathish,
              toolchain: Optional[Pathish] = None,
              build_root: Optional[Pathish] = None,
              jobs: Optional[int] = None,
              tweaks_dir: Optional[Pathish] = None,
              with_tests: bool = True,
              repos: Sequence[Pathish] = (),
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
                (f'--use-repo={r}' for r in repos),
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
                     out: Optional[Pathish] = None,
                     more_args: proc.CommandLine = ()) -> None:
        """
        Run 'dds compile-file' for the given paths.
        """
        toolchain = toolchain or tc_mod.get_default_audit_toolchain()
        self.run([
            'compile-file',
            paths,
            f'--tweaks-dir={tweaks_dir}' if tweaks_dir else (),
            f'--toolchain={toolchain}',
            f'--project={project_dir}',
            f'--out={out}',
            more_args,
        ])

    def build_deps(self,
                   args: proc.CommandLine,
                   *,
                   repos: Sequence[Pathish] = (),
                   toolchain: Optional[Pathish] = None) -> None:
        toolchain = toolchain or tc_mod.get_default_audit_toolchain()
        self.run([
            'build-deps',
            '-ltrace',
            f'--toolchain={toolchain}',
            (f'--use-repo={r}' for r in repos),
            args,
        ])
