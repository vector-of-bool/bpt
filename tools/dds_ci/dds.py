import multiprocessing
import shutil
from pathlib import Path
from typing import Optional

from . import paths, proc, toolchain as tc_mod
from dds_ci.util import Pathish


class DDSWrapper:
    """
    Wraps a 'dds' executable with some convenience APIs that invoke various
    'dds' subcommands.
    """
    def __init__(self,
                 path: Path,
                 *,
                 repo_dir: Optional[Pathish] = None,
                 catalog_path: Optional[Pathish] = None,
                 default_cwd: Optional[Pathish] = None) -> None:
        self.path = path
        self.repo_dir = Path(repo_dir or (paths.PREBUILT_DIR / 'ci-repo'))
        self.catalog_path = Path(catalog_path or (self.repo_dir.parent / 'ci-catalog.db'))
        self.default_cwd = default_cwd or Path.cwd()

    def clone(self) -> 'DDSWrapper':
        return DDSWrapper(self.path,
                          repo_dir=self.repo_dir,
                          catalog_path=self.catalog_path,
                          default_cwd=self.default_cwd)

    @property
    def catalog_path_arg(self) -> str:
        """The arguments for --catalog"""
        return f'--catalog={self.catalog_path}'

    @property
    def repo_dir_arg(self) -> str:
        """The arguments for --repo-dir"""
        return f'--repo-dir={self.repo_dir}'

    def set_repo_scratch(self, path: Pathish) -> None:
        self.repo_dir = Path(path) / 'data'
        self.catalog_path = Path(path) / 'catalog.db'

    def clean(self, *, build_dir: Optional[Path] = None, repo: bool = True, catalog: bool = True) -> None:
        """
        Clean out prior executable output, including repos, catalog, and
        the build results at 'build_dir', if given.
        """
        if build_dir and build_dir.exists():
            shutil.rmtree(build_dir)
        if repo and self.repo_dir.exists():
            shutil.rmtree(self.repo_dir)
        if catalog and self.catalog_path.exists():
            self.catalog_path.unlink()

    def run(self, args: proc.CommandLine, *, cwd: Optional[Pathish] = None) -> None:
        """Execute the 'dds' executable with the given arguments"""
        proc.check_run([self.path, args], cwd=cwd or self.default_cwd)

    def catalog_json_import(self, path: Path) -> None:
        """Run 'catalog import' to import the given JSON. Only applicable to older 'dds'"""
        self.run(['catalog', 'import', self.catalog_path_arg, f'--json={path}'])

    def catalog_get(self, what: str) -> None:
        self.run(['catalog', 'get', self.catalog_path_arg, what])

    def repo_add(self, url: str) -> None:
        self.run(['repo', 'add', self.catalog_path_arg, url, '--update'])

    def repo_import(self, sdist: Path) -> None:
        self.run(['repo', self.repo_dir_arg, 'import', sdist])

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
        toolchain = toolchain or tc_mod.get_default_test_toolchain()
        jobs = jobs or multiprocessing.cpu_count() + 2
        self.run([
            'build',
            f'--toolchain={toolchain}',
            self.repo_dir_arg,
            self.catalog_path_arg,
            f'--jobs={jobs}',
            f'--project-dir={root}',
            f'--out={build_root}',
        ])

    def build_deps(self, args: proc.CommandLine, *, toolchain: Optional[Path] = None) -> None:
        toolchain = toolchain or tc_mod.get_default_test_toolchain()
        self.run([
            'build-deps',
            f'--toolchain={toolchain}',
            self.catalog_path_arg,
            self.repo_dir_arg,
            args,
        ])
