import os
import itertools
from contextlib import contextmanager, ExitStack
from pathlib import Path
from typing import Iterable, Union, Any, Dict, NamedTuple, ContextManager
import subprocess
import shutil

import pytest

from dds_ci import proc

from . import fileutil


class DDS:
    def __init__(self, dds_exe: Path, test_dir: Path, project_dir: Path,
                 scope: ExitStack) -> None:
        self.dds_exe = dds_exe
        self.test_dir = test_dir
        self.source_root = project_dir
        self.scratch_dir = project_dir / '_test_scratch'
        self.scope = scope
        self.scope.callback(self.cleanup)

    @property
    def repo_dir(self) -> Path:
        return self.scratch_dir / 'repo'

    @property
    def deps_build_dir(self) -> Path:
        return self.scratch_dir / 'deps-build'

    @property
    def build_dir(self) -> Path:
        return self.scratch_dir / 'build'

    @property
    def lmi_path(self) -> Path:
        return self.scratch_dir / 'INDEX.lmi'

    def cleanup(self):
        if self.scratch_dir.exists():
            shutil.rmtree(self.scratch_dir)

    def run_unchecked(self, cmd: proc.CommandLine, *,
                      cwd: Path = None) -> subprocess.CompletedProcess:
        full_cmd = itertools.chain([self.dds_exe], cmd)
        return proc.run(full_cmd, cwd=cwd or self.source_root)

    def run(self, cmd: proc.CommandLine, *,
            cwd: Path = None) -> subprocess.CompletedProcess:
        cmdline = list(proc.flatten_cmd(cmd))
        res = self.run_unchecked(cmd)
        if res.returncode != 0:
            raise subprocess.CalledProcessError(
                res.returncode, [self.dds_exe] + cmdline, res.stdout)
        return res

    @property
    def repo_dir_arg(self) -> str:
        return f'--repo-dir={self.repo_dir}'

    @property
    def project_dir_arg(self) -> str:
        return f'--project-dir={self.source_root}'

    def deps_ls(self) -> subprocess.CompletedProcess:
        return self.run(['deps', 'ls'])

    def deps_get(self) -> subprocess.CompletedProcess:
        return self.run([
            'deps',
            'get',
            self.repo_dir_arg,
        ])

    def deps_build(self, *,
                   toolchain: str = None) -> subprocess.CompletedProcess:
        return self.run([
            'deps',
            'build',
            f'--toolchain={toolchain or self.default_builtin_toolchain}',
            self.repo_dir_arg,
            f'--deps-build-dir={self.deps_build_dir}',
            f'--lmi-path={self.lmi_path}',
        ])

    def build(self,
              *,
              toolchain: str = None,
              apps: bool = True,
              warnings: bool = True,
              tests: bool = True,
              export: bool = False) -> subprocess.CompletedProcess:
        return self.run([
            'build',
            f'--out={self.build_dir}',
            ['--tests'] if tests else [],
            ['--apps'] if apps else [],
            ['--warnings'] if warnings else [],
            ['--export'] if export else [],
            f'--toolchain={toolchain or self.default_builtin_toolchain}',
            f'--lm-index={self.lmi_path}',
            self.project_dir_arg,
        ])

    def sdist_create(self) -> subprocess.CompletedProcess:
        return self.run([
            'sdist',
            'create',
            self.project_dir_arg,
            f'--out={self.build_dir / "created-sdist.sds"}',
        ])

    def sdist_export(self) -> subprocess.CompletedProcess:
        return self.run([
            'sdist',
            'export',
            self.project_dir_arg,
            self.repo_dir_arg,
        ])

    @property
    def default_builtin_toolchain(self) -> str:
        if os.name == 'posix':
            return ':gcc-9'
        elif os.name == 'nt':
            return ':msvc'
        else:
            raise RuntimeError(
                f'No default builtin toolchain defined for tests on platform "{os.name}"'
            )

    @property
    def exe_suffix(self) -> str:
        if os.name == 'posix':
            return ''
        elif os.name == 'nt':
            return '.exe'
        else:
            raise RuntimeError(
                f'We don\'t know the executable suffix for the platform "{os.name}"'
            )

    def set_contents(self, path: Union[str, Path],
                     content: bytes) -> ContextManager[Path]:
        return fileutil.set_contents(self.source_root / path, content)


@contextmanager
def scoped_dds(test_dir: Path, project_dir: Path, name: str):
    dds_exe = Path(__file__).absolute().parent.parent / '_build/dds'
    if os.name == 'nt':
        dds_exe = dds_exe.with_suffix('.exe')
    with ExitStack() as scope:
        yield DDS(dds_exe, test_dir, project_dir, scope)


class DDSFixtureParams(NamedTuple):
    ident: str
    subdir: Union[Path, str]


def dds_fixture_conf(*argsets: DDSFixtureParams):
    args = list(argsets)
    return pytest.mark.parametrize(
        'dds', args, indirect=True, ids=[p.ident for p in args])


def dds_fixture_conf_1(subdir: Union[Path, str]):
    params = DDSFixtureParams(ident='only', subdir=subdir)
    return pytest.mark.parametrize('dds', [params], indirect=True, ids=['.'])
