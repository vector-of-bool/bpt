import enum
from pathlib import Path
import os
from contextlib import contextmanager
from typing import Iterator, Optional, Mapping
import sys
import urllib.request
import platform
import shutil

from . import paths, proc
from .dds import DDSWrapper
from .paths import new_tempdir


class BootstrapMode(enum.Enum):
    """How should be bootstrap our prior DDS executable?"""
    #: Downlaod one from GitHub
    Download = 'download'
    #: Build one from source
    Build = 'build'
    #: Skip bootstrapping. Assume it already exists.
    Skip = 'skip'
    #: If the prior executable exists, skip, otherwise download
    Lazy = 'lazy'


def _do_bootstrap_download() -> Path:
    filename = {
        'win32': 'dds-win-x64.exe',
        'linux': 'dds-linux-x64',
        'darwin': 'dds-macos-x64',
        'freebsd11': 'dds-freebsd-x64',
        'freebsd12': 'dds-freebsd-x64',
    }.get(sys.platform)
    if filename is None:
        raise RuntimeError(f'We do not have a prebuilt DDS binary for the "{sys.platform}" platform')
    url = f'https://github.com/vector-of-bool/dds/releases/download/0.1.0-alpha.6/{filename}'

    print(f'Downloading prebuilt DDS executable: {url}')
    stream = urllib.request.urlopen(url)
    paths.PREBUILT_DDS.parent.mkdir(exist_ok=True, parents=True)
    with paths.PREBUILT_DDS.open('wb') as fd:
        while True:
            buf = stream.read(1024 * 4)
            if not buf:
                break
            fd.write(buf)

    if sys.platform != 'win32':
        # Mark the binary executable. By default it won't be
        mode = paths.PREBUILT_DDS.stat().st_mode
        mode |= 0b001_001_001
        paths.PREBUILT_DDS.chmod(mode)

    return paths.PREBUILT_DDS


@contextmanager
def pin_exe(fpath: Path) -> Iterator[Path]:
    """
    Create a copy of 'fpath' at an unspecified location, and yield that path.

    This is needed if the executable would overwrite itself.
    """
    with new_tempdir() as tdir:
        tfile = tdir / 'previous-dds.exe'
        shutil.copy2(fpath, tfile)
        yield tfile


@contextmanager
def get_bootstrap_exe(mode: BootstrapMode) -> Iterator[DDSWrapper]:
    """Context manager that yields a DDSWrapper around a prior 'dds' executable"""
    if mode is BootstrapMode.Lazy:
        f = paths.PREBUILT_DDS
        if not f.exists():
            _do_bootstrap_download()
    elif mode is BootstrapMode.Download:
        f = _do_bootstrap_download()
    elif mode is BootstrapMode.Build:
        f = _do_bootstrap_build()
    elif mode is BootstrapMode.Skip:
        f = paths.PREBUILT_DDS

    with pin_exe(f) as dds:
        yield DDSWrapper(dds)


def _do_bootstrap_build() -> Path:
    return _bootstrap_p6()


def _bootstrap_p6() -> Path:
    prev_dds = _bootstrap_alpha_4()
    p6_dir = paths.PREBUILT_DIR / 'p6'
    ret_dds = _dds_in(p6_dir)
    if ret_dds.exists():
        return ret_dds

    _clone_self_at(p6_dir, '0.1.0-alpha.6')
    tc = 'msvc-rel.jsonc' if platform.system() == 'Windows' else 'gcc-9-rel.jsonc'

    catalog_arg = f'--catalog={p6_dir}/_catalog.db'
    repo_arg = f'--repo-dir={p6_dir}/_repo'

    proc.check_run(
        [prev_dds, 'catalog', 'import', catalog_arg, '--json=old-catalog.json'],
        cwd=p6_dir,
    )
    proc.check_run(
        [
            prev_dds,
            'build',
            catalog_arg,
            repo_arg,
            ('--toolchain', p6_dir / 'tools' / tc),
        ],
        cwd=p6_dir,
    )
    return ret_dds


def _bootstrap_alpha_4() -> Path:
    prev_dds = _bootstrap_alpha_3()
    a4_dir = paths.PREBUILT_DIR / 'alpha-4'
    ret_dds = _dds_in(a4_dir)
    if ret_dds.exists():
        return ret_dds

    _clone_self_at(a4_dir, '0.1.0-alpha.4')
    build_py = a4_dir / 'tools/build.py'
    proc.check_run(
        [
            sys.executable,
            '-u',
            build_py,
        ],
        env=_prev_dds_env(prev_dds),
        cwd=a4_dir,
    )
    return ret_dds


def _bootstrap_alpha_3() -> Path:
    prev_dds = _bootstrap_p5()
    a3_dir = paths.PREBUILT_DIR / 'alpha-3'
    ret_dds = _dds_in(a3_dir)
    if ret_dds.exists():
        return ret_dds

    _clone_self_at(a3_dir, '0.1.0-alpha.3')
    build_py = a3_dir / 'tools/build.py'
    proc.check_run(
        [
            sys.executable,
            '-u',
            build_py,
        ],
        env=_prev_dds_env(prev_dds),
        cwd=a3_dir,
    )
    return ret_dds


def _bootstrap_p5() -> Path:
    prev_dds = _bootstrap_p4()
    p5_dir = paths.PREBUILT_DIR / 'p5'
    ret_dds = _dds_in(p5_dir)
    if ret_dds.exists():
        return ret_dds

    _clone_self_at(p5_dir, 'bootstrap-p5')
    build_py = p5_dir / 'tools/build.py'
    proc.check_run(
        [
            sys.executable,
            '-u',
            build_py,
        ],
        env=_prev_dds_env(prev_dds),
        cwd=p5_dir,
    )
    return ret_dds


def _bootstrap_p4() -> Path:
    prev_dds = _bootstrap_p1()
    p4_dir = paths.PREBUILT_DIR / 'p4'
    p4_dir.mkdir(exist_ok=True, parents=True)
    ret_dds = _dds_in(p4_dir)
    if ret_dds.exists():
        return ret_dds

    _clone_self_at(p4_dir, 'bootstrap-p4')
    build_py = p4_dir / 'tools/build.py'
    proc.check_run(
        [
            sys.executable,
            '-u',
            build_py,
            '--cxx=cl.exe' if platform.system() == 'Windows' else '--cxx=g++-8',
        ],
        env=_prev_dds_env(prev_dds),
    )
    return ret_dds


def _bootstrap_p1() -> Path:
    p1_dir = paths.PREBUILT_DIR / 'p1'
    p1_dir.mkdir(exist_ok=True, parents=True)
    ret_dds = _dds_in(p1_dir)
    if ret_dds.exists():
        return ret_dds

    _clone_self_at(p1_dir, 'bootstrap-p1')
    build_py = p1_dir / 'tools/build.py'
    proc.check_run([
        sys.executable,
        '-u',
        build_py,
        '--cxx=cl.exe' if platform.system() == 'Windows' else '--cxx=g++-8',
    ])
    _build_prev(p1_dir)
    return ret_dds


def _clone_self_at(dir: Path, ref: str) -> None:
    if dir.is_dir():
        shutil.rmtree(dir)
    dir.mkdir(exist_ok=True, parents=True)
    proc.check_run(['git', 'clone', '-qq', paths.PROJECT_ROOT, f'--branch={ref}', dir])


def _dds_in(dir: Path) -> Path:
    return dir.joinpath('_build/dds' + paths.EXE_SUFFIX)


def _prev_dds_env(dds: Path) -> Mapping[str, str]:
    env = os.environ.copy()
    env.update({'DDS_BOOTSTRAP_PREV_EXE': str(dds)})
    return env


def _build_prev(dir: Path, prev_dds: Optional[Path] = None) -> None:
    build_py = dir / 'tools/build.py'
    proc.check_run(
        [
            sys.executable,
            '-u',
            build_py,
            '--cxx=cl.exe' if platform.system() == 'Windows' else '--cxx=g++-8',
        ],
        cwd=dir,
        env=None if prev_dds is None else os.environ.clone().update({'DDS_BOOTSTRAP_PREV_EXE',
                                                                     str(prev_dds)}),
    )
