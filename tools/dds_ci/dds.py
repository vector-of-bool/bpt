from pathlib import Path
import multiprocessing
import shutil

from . import proc
from . import paths


class DDSWrapper:
    """
    Wraps a 'dds' executable with some convenience APIs that invoke various
    'dds' subcommands.
    """
    def __init__(self, path: Path) -> None:
        self.path = path
        self.repo_dir = paths.PREBUILT_DIR / 'ci-repo'
        self.catalog_path = paths.PREBUILT_DIR / 'ci-catalog.db'

    @property
    def catalog_path_arg(self):
        """The arguments for --catalog"""
        return f'--catalog={self.catalog_path}'

    @property
    def repo_dir_arg(self):
        """The arguments for --repo-dir"""
        return f'--repo-dir={self.repo_dir}'

    def clean(self, *, build_dir: Path = None, repo=True, catalog=True):
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

    def run(self, args: proc.CommandLine) -> None:
        """Execute the 'dds' executable with the given arguments"""
        proc.check_run([self.path, args])  # type: ignore

    def catalog_json_import(self, path: Path) -> None:
        """Run 'catalog import' to import the given JSON. Only applicable to older 'dds'"""
        self.run(['catalog', 'import', self.catalog_path_arg, f'--json={path}'])

    def build(self, *, toolchain: Path, root: Path, build_root: Path = None, jobs: int = None) -> None:
        """
        Run 'dds build' with the given arguments.

        :param toolchain: The toolchain to use for the build.
        :param root: The root project directory.
        :param build_root: The root directory where the output will be written.
        :param jobs: The number of jobs to use. Default is CPU-count + 2
        """
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
