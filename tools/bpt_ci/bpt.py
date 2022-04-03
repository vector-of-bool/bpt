from __future__ import annotations

import copy
import multiprocessing
import os
import shutil
from pathlib import Path
from typing import Iterable, TypeVar

from bpt_ci.util import Pathish

from . import proc
from . import toolchain as tc_mod

T = TypeVar('T')
"""
A generic unbounded type parameter
"""

class BPTWrapper:
    """
    Wraps a 'bpt' executable with some convenience APIs that invoke various
    'bpt' subcommands.
    """

    def __init__(self, path: Path, *, crs_cache_dir: Pathish | None = None, default_cwd: Pathish | None = None) -> None:
        self.path = path
        "The path to the wrapped ``bpt`` executable"
        self.default_cwd = Path(default_cwd or Path.cwd())
        "The directory in which ``bpt`` commands will execute unless otherwise specified"
        self.crs_cache_dir = crs_cache_dir
        "The directory in which the CRS cache data will be stored"

    def clone(self) -> BPTWrapper:
        "Create a clone of this object"
        return copy.deepcopy(self)

    @property
    def always_args(self) -> proc.CommandLine:
        """Arguments that are always given to every bpt subcommand"""
        return [self.crs_cache_dir_arg]

    @property
    def crs_cache_dir_arg(self) -> proc.CommandLine:
        """The arguments for ``--crs-cache-dir``"""
        if self.crs_cache_dir is not None:
            return [f'--crs-cache-dir={self.crs_cache_dir}']
        return []

    def clean(self, *, build_dir: Path | None = None, crs_cache: bool = True) -> None:
        """
        Clean out prior executable output, including the CRS cache and
        the build results at 'build_dir', if given.
        """
        if build_dir and build_dir.exists():
            shutil.rmtree(build_dir)
        if crs_cache and self.crs_cache_dir:
            p = Path(self.crs_cache_dir)
            if p.exists():
                shutil.rmtree(p)

    def run(self, args: proc.CommandLine, *, cwd: Pathish | None = None, timeout: float | None = None) -> None:
        """
        Execute the 'bpt' executable with the given arguments

        :param args: The command arguments to give to ``bpt``.
        :param cwd: The working directory of the subprocess.
        :param timeout: A timeout for the subprocess's execution.
        """
        env = os.environ.copy()
        proc.check_run([self.path, self.always_args, args], cwd=cwd or self.default_cwd, env=env, timeout=timeout)

    def pkg_prefetch(self, *, repos: Iterable[Pathish], pkgs: Iterable[str] = ()) -> None:
        "Execute the ``bpt pkg prefetch`` subcommand"
        self.run(['pkg', 'prefetch', '--no-default-repo', (f'--use-repo={r}' for r in repos), pkgs])

    def pkg_solve(self, *, repos: Iterable[Pathish], pkgs: Iterable[str]) -> None:
        "Execute the ``bpt pkg solve`` subcommand"
        self.run(['pkg', 'solve', '--no-default-repo', (f'--use-repo={r}' for r in repos), pkgs])

    def build(self,
              *,
              root: Pathish,
              toolchain: Pathish | None = None,
              build_root: Pathish | None = None,
              jobs: int | None = None,
              tweaks_dir: Pathish | None = None,
              with_tests: bool = True,
              repos: Iterable[Pathish] = (),
              more_args: proc.CommandLine | None = None,
              timeout: float | None = None) -> None:
        """
        Run 'bpt build' with the given arguments.

        :param root: The root project directory.
        :param toolchain: The toolchain to use for the build.
        :param build_root: The root directory where the output will be written.
        :param jobs: The number of jobs to use. Default is CPU-count + 2
        :param tweaks_dir: Set the tweaks-dir for the build
        :param with_tests: Toggle building and executing of tests.
        :param repos: Repositories to use during the build.
        :param more_args: Additional command-line arguments.
        :param timeout: Timeout for the build subprocess.
        """
        toolchain = toolchain or tc_mod.get_default_audit_toolchain()
        jobs = jobs or multiprocessing.cpu_count() + 2
        self.run(
            [
                'build',
                '--no-default-repo',
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
                     tweaks_dir: Pathish | None = None,
                     toolchain: Pathish | None = None,
                     root: Pathish,
                     build_root: Pathish | None = None,
                     more_args: proc.CommandLine = ()) -> None:
        """
        Run 'bpt compile-file' for the given paths.

        .. seealso: :func:`build` for additional parameter information
        """
        toolchain = toolchain or tc_mod.get_default_audit_toolchain()
        self.run([
            'compile-file',
            paths,
            f'--tweaks-dir={tweaks_dir}' if tweaks_dir else (),
            f'--toolchain={toolchain}',
            f'--project={root}',
            f'--out={build_root}',
            more_args,
        ])

    def build_deps(self,
                   args: proc.CommandLine,
                   *,
                   repos: Iterable[Pathish] = (),
                   toolchain: Pathish | None = None) -> None:
        """
        run the ``bpt build-deps`` subcommand.

        .. seealso: :func:`build` for additional parameter information.
        """
        toolchain = toolchain or tc_mod.get_default_audit_toolchain()
        self.run([
            'build-deps',
            '-ltrace',
            f'--toolchain={toolchain}',
            (f'--use-repo={r}' for r in repos),
            args,
        ])
