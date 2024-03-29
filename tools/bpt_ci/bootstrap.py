import enum
import os
import platform
import shutil
import sys
import urllib.request
from contextlib import contextmanager
from pathlib import Path
from typing import Iterator, Mapping, Optional

from . import paths, proc
from .bpt import BPTWrapper
from .paths import new_tempdir


class BootstrapMode(enum.Enum):
    """How should be bootstrap our prior BPT executable?"""
    Download = 'download'
    'Downlaod one from GitHub'
    Build = 'build'
    'Build one from source'
    Skip = 'skip'
    'Skip bootstrapping. Assume it already exists.'
    Lazy = 'lazy'
    'If the prior executable exists, skip, otherwise download'


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

    print(f'Downloading prebuilt BPT executable: {url}')
    with urllib.request.urlopen(url) as stream:
        paths.PREBUILT_BPT.parent.mkdir(exist_ok=True, parents=True)
        with paths.PREBUILT_BPT.open('wb') as fd:
            while True:
                buf = stream.read(1024 * 4)
                if not buf:
                    break
                fd.write(buf)

    if sys.platform != 'win32':
        # Mark the binary executable. By default it won't be
        mode = paths.PREBUILT_BPT.stat().st_mode
        mode |= 0b001_001_001
        paths.PREBUILT_BPT.chmod(mode)

    return paths.PREBUILT_BPT


@contextmanager
def pin_exe(fpath: Path) -> Iterator[Path]:
    """
    Create a copy of the file at ``fpath`` at an unspecified location, and
    yield that path.

    This is needed if the executable would overwrite itself.
    """
    with new_tempdir() as tdir:
        tfile = tdir / 'previous-bpt.exe'
        shutil.copy2(fpath, tfile)
        yield tfile


def get_bootstrap_exe(mode: BootstrapMode) -> BPTWrapper:
    """Obtain a ``bpt`` executable for the given bootstrapping mode"""
    if mode is BootstrapMode.Lazy:
        f = paths.PREBUILT_BPT
        if not f.exists():
            _do_bootstrap_download()
    elif mode is BootstrapMode.Download:
        f = _do_bootstrap_download()
    elif mode is BootstrapMode.Build:
        f = _do_bootstrap_build()
    elif mode is BootstrapMode.Skip:
        f = paths.PREBUILT_BPT

    return BPTWrapper(f)


def _do_bootstrap_build() -> Path:
    return _bootstrap_p6()


def _bootstrap_p6() -> Path:
    prev_bpt = _bootstrap_alpha_4()
    p6_dir = paths.PREBUILT_DIR / 'p6'
    ret_bpt = _bpt_in(p6_dir)
    if ret_bpt.exists():
        return ret_bpt

    _clone_self_at(p6_dir, '0.1.0-alpha.6-bootstrap')
    tc = 'msvc-rel.jsonc' if platform.system() == 'Windows' else 'gcc-10-rel.jsonc'

    catalog_arg = f'--catalog={p6_dir}/_catalog.db'
    repo_arg = f'--repo-dir={p6_dir}/_repo'

    proc.check_run(
        [prev_bpt, 'catalog', 'import', catalog_arg, '--json=old-catalog.json'],
        cwd=p6_dir,
    )
    proc.check_run(
        [
            prev_bpt,
            'build',
            catalog_arg,
            repo_arg,
            ('--toolchain', p6_dir / 'tools' / tc),
        ],
        cwd=p6_dir,
    )
    return ret_bpt


def _bootstrap_alpha_4() -> Path:
    prev_bpt = _bootstrap_alpha_3()
    a4_dir = paths.PREBUILT_DIR / 'alpha-4'
    ret_bpt = _bpt_in(a4_dir)
    if ret_bpt.exists():
        return ret_bpt

    _clone_self_at(a4_dir, '0.1.0-alpha.4')
    build_py = a4_dir / 'tools/build.py'
    proc.check_run(
        [
            sys.executable,
            '-u',
            build_py,
        ],
        env=_prev_bpt_env(prev_bpt),
        cwd=a4_dir,
    )
    return ret_bpt


def _bootstrap_alpha_3() -> Path:
    prev_bpt = _bootstrap_p5()
    a3_dir = paths.PREBUILT_DIR / 'alpha-3'
    ret_bpt = _bpt_in(a3_dir)
    if ret_bpt.exists():
        return ret_bpt

    _clone_self_at(a3_dir, '0.1.0-alpha.3')
    build_py = a3_dir / 'tools/build.py'
    proc.check_run(
        [
            sys.executable,
            '-u',
            build_py,
        ],
        env=_prev_bpt_env(prev_bpt),
        cwd=a3_dir,
    )
    return ret_bpt


def _bootstrap_p5() -> Path:
    prev_bpt = _bootstrap_p4()
    p5_dir = paths.PREBUILT_DIR / 'p5'
    ret_bpt = _bpt_in(p5_dir)
    if ret_bpt.exists():
        return ret_bpt

    _clone_self_at(p5_dir, 'bootstrap-p5.2')
    build_py = p5_dir / 'tools/build.py'
    proc.check_run(
        [
            sys.executable,
            '-u',
            build_py,
        ],
        env=_prev_bpt_env(prev_bpt),
        cwd=p5_dir,
    )
    return ret_bpt


def _bootstrap_p4() -> Path:
    prev_bpt = _bootstrap_p1()
    p4_dir = paths.PREBUILT_DIR / 'p4'
    p4_dir.mkdir(exist_ok=True, parents=True)
    ret_bpt = _bpt_in(p4_dir)
    if ret_bpt.exists():
        return ret_bpt

    _clone_self_at(p4_dir, 'bootstrap-p4.2')
    build_py = p4_dir / 'tools/build.py'
    proc.check_run(
        [
            sys.executable,
            '-u',
            build_py,
            '--cxx=cl.exe' if platform.system() == 'Windows' else '--cxx=g++-8',
        ],
        env=_prev_bpt_env(prev_bpt),
    )
    return ret_bpt


def _bootstrap_p1() -> Path:
    p1_dir = paths.PREBUILT_DIR / 'p1'
    p1_dir.mkdir(exist_ok=True, parents=True)
    ret_bpt = _bpt_in(p1_dir)
    if ret_bpt.exists():
        return ret_bpt

    _clone_self_at(p1_dir, 'bootstrap-p1.2')
    build_py = p1_dir / 'tools/build.py'
    proc.check_run([
        sys.executable,
        '-u',
        build_py,
        '--cxx=cl.exe' if platform.system() == 'Windows' else '--cxx=g++-8',
    ])
    _build_prev(p1_dir)
    return ret_bpt


def _clone_self_at(dirpath: Path, ref: str) -> None:
    if dirpath.is_dir():
        shutil.rmtree(dirpath)
    dirpath.mkdir(exist_ok=True, parents=True)
    proc.check_run(['git', 'clone', '-qq', paths.PROJECT_ROOT, f'--branch={ref}', dirpath])


def _bpt_in(dirpath: Path) -> Path:
    return dirpath.joinpath('_build/bpt' + paths.EXE_SUFFIX)


def _prev_bpt_env(bpt: Path) -> Mapping[str, str]:
    env = os.environ.copy()
    env.update({'BPT_BOOTSTRAP_PREV_EXE': str(bpt)})
    return env


def _build_prev(dirpath: Path, prev_bpt: Optional[Path] = None) -> None:
    build_py = dirpath / 'tools/build.py'
    env: Optional[Mapping[str, str]] = None
    if prev_bpt is not None:
        env = os.environ.copy()
        env['BPT_BOOSTRAP_PREV_EXE'] = str(prev_bpt)
    proc.check_run(
        [
            sys.executable,
            '-u',
            build_py,
            '--cxx=cl.exe' if platform.system() == 'Windows' else '--cxx=g++-8',
        ],
        cwd=dirpath,
        env=env,
    )
