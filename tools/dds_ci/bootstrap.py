import enum
from pathlib import Path
from contextlib import contextmanager
from typing import Iterator, ContextManager
import sys
import urllib.request
import shutil
import tempfile

from . import paths
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
    url = f'https://github.com/vector-of-bool/dds/releases/download/0.1.0-alpha.4/{filename}'

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
