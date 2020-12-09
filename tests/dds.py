import os
import itertools
from contextlib import contextmanager, ExitStack
from pathlib import Path
from typing import Union, NamedTuple, ContextManager, Optional, Iterator, TypeVar
import shutil

import pytest
import _pytest

from dds_ci import proc, toolchain as tc_mod

from . import fileutil

T = TypeVar('T')


class DDS:
    def __init__(self, dds_exe: Path, test_dir: Path, project_dir: Path, scope: ExitStack) -> None:
        self.dds_exe = dds_exe
        self.test_dir = test_dir
        self.source_root = project_dir
        self.scratch_dir = project_dir / '_test_scratch/Ю́рий Алексе́евич Гага́рин'
        self.scope = scope
        self.scope.callback(self.cleanup)

    @property
    def repo_dir(self) -> Path:
        return self.scratch_dir / 'repo'

    @property
    def catalog_path(self) -> Path:
        return self.scratch_dir / 'catalog.db'

    @property
    def deps_build_dir(self) -> Path:
        return self.scratch_dir / 'deps-build'

    @property
    def build_dir(self) -> Path:
        return self.scratch_dir / 'build'

    @property
    def lmi_path(self) -> Path:
        return self.scratch_dir / 'INDEX.lmi'

    def cleanup(self) -> None:
        if self.scratch_dir.exists():
            shutil.rmtree(self.scratch_dir)

    def run_unchecked(self, cmd: proc.CommandLine, *, cwd: Optional[Path] = None) -> proc.ProcessResult:
        full_cmd = itertools.chain([self.dds_exe, '-ltrace'], cmd)
        return proc.run(full_cmd, cwd=cwd or self.source_root)  # type: ignore

    def run(self, cmd: proc.CommandLine, *, cwd: Optional[Path] = None, check: bool = True) -> proc.ProcessResult:
        full_cmd = itertools.chain([self.dds_exe, '-ltrace'], cmd)
        return proc.run(full_cmd, cwd=cwd, check=check)  # type: ignore

    @property
    def repo_dir_arg(self) -> str:
        return f'--repo-dir={self.repo_dir}'

    @property
    def project_dir_arg(self) -> str:
        return f'--project-dir={self.source_root}'

    @property
    def catalog_path_arg(self) -> str:
        return f'--catalog={self.catalog_path}'

    def build_deps(self, args: proc.CommandLine, *, toolchain: Optional[str] = None) -> proc.ProcessResult:
        return self.run([
            'build-deps',
            f'--toolchain={toolchain or tc_mod.get_default_test_toolchain()}',
            self.catalog_path_arg,
            self.repo_dir_arg,
            f'--out={self.deps_build_dir}',
            f'--lmi-path={self.lmi_path}',
            args,
        ])

    def repo_add(self, url: str) -> None:
        self.run(['repo', 'add', url, '--update', self.catalog_path_arg])

    def build(self,
              *,
              toolchain: Optional[str] = None,
              apps: bool = True,
              warnings: bool = True,
              catalog_path: Optional[Path] = None,
              tests: bool = True,
              more_args: proc.CommandLine = (),
              check: bool = True) -> proc.ProcessResult:
        catalog_path = catalog_path or self.catalog_path
        return self.run(
            [
                'build',
                f'--out={self.build_dir}',
                f'--toolchain={toolchain or tc_mod.get_default_test_toolchain()}',
                f'--catalog={catalog_path}',
                f'--repo-dir={self.repo_dir}',
                ['--no-tests'] if not tests else [],
                ['--no-apps'] if not apps else [],
                ['--no-warnings'] if not warnings else [],
                self.project_dir_arg,
                more_args,
            ],
            check=check,
        )

    def sdist_create(self) -> proc.ProcessResult:
        self.build_dir.mkdir(exist_ok=True, parents=True)
        return self.run(['sdist', 'create', self.project_dir_arg], cwd=self.build_dir)

    def sdist_export(self) -> proc.ProcessResult:
        return self.run([
            'sdist',
            'export',
            self.project_dir_arg,
            self.repo_dir_arg,
        ])

    def repo_import(self, sdist: Path) -> proc.ProcessResult:
        return self.run(['repo', self.repo_dir_arg, 'import', sdist])

    def catalog_create(self) -> proc.ProcessResult:
        self.scratch_dir.mkdir(parents=True, exist_ok=True)
        return self.run(['catalog', 'create', f'--catalog={self.catalog_path}'], cwd=self.test_dir)

    def catalog_get(self, req: str) -> proc.ProcessResult:
        return self.run([
            'catalog',
            'get',
            f'--catalog={self.catalog_path}',
            f'--out-dir={self.scratch_dir}',
            req,
        ])

    def set_contents(self, path: Union[str, Path], content: bytes) -> ContextManager[Path]:
        return fileutil.set_contents(self.source_root / path, content)


@contextmanager
def scoped_dds(dds_exe: Path, test_dir: Path, project_dir: Path) -> Iterator[DDS]:
    if os.name == 'nt':
        dds_exe = dds_exe.with_suffix('.exe')
    with ExitStack() as scope:
        yield DDS(dds_exe, test_dir, project_dir, scope)


class DDSFixtureParams(NamedTuple):
    ident: str
    subdir: Union[Path, str]


def dds_fixture_conf(*argsets: DDSFixtureParams) -> _pytest.mark.MarkDecorator:
    args = list(argsets)
    return pytest.mark.parametrize('dds', args, indirect=True, ids=[p.ident for p in args])


def dds_fixture_conf_1(subdir: Union[Path, str]) -> _pytest.mark.MarkDecorator:
    params = DDSFixtureParams(ident='only', subdir=subdir)
    return pytest.mark.parametrize('dds', [params], indirect=True, ids=['.'])
