import os
import shutil
import itertools
import tempfile
from contextlib import contextmanager
from pathlib import Path
from typing import Iterator, Optional

# The root directory of the dds project
PROJECT_ROOT = Path(__file__).absolute().parent.parent.parent
#: The <repo>/tools directory
TOOLS_DIR = PROJECT_ROOT / 'tools'
#: The default build directory
BUILD_DIR = PROJECT_ROOT / '_build'
#: The directory were w prebuild/bootstrapped results will go, and scratch space for the build
PREBUILT_DIR = PROJECT_ROOT / '_prebuilt'
#: THe suffix of executable files on this system
EXE_SUFFIX = '.exe' if os.name == 'nt' else ''
#: The path to the prebuilt 'dds' executable
PREBUILT_DDS = (PREBUILT_DIR / 'dds').with_suffix(EXE_SUFFIX)
#: The path to the main built 'dds' executable
CUR_BUILT_DDS = (BUILD_DIR / 'dds').with_suffix(EXE_SUFFIX)


@contextmanager
def new_tempdir() -> Iterator[Path]:
    """
    Create and yield a new temporary directory, which will be destroyed on
    context-manager exit
    """
    tdir = Path(tempfile.mkdtemp())
    try:
        yield tdir
    finally:
        shutil.rmtree(tdir)


def find_exe(name: str) -> Optional[Path]:
    """
    Find a file on the system by searching through the PATH environment variable.
    """
    sep = ';' if os.name == 'nt' else ':'
    paths = os.environ['PATH'].split(sep)
    exts = os.environ['PATHEXT'].split(';') if os.name == 'nt' else ['']
    for dirpath, ext in itertools.product(paths, exts):
        cand = Path(dirpath) / (name + ext)
        if cand.is_file():
            return cand
    return None
