import os
import shutil
import itertools
import tempfile
from contextlib import contextmanager
from pathlib import Path
from typing import Iterator, Optional

PROJECT_ROOT = Path(__file__).absolute().parent.parent.parent
'The root directory of the dds project'
TOOLS_DIR = PROJECT_ROOT / 'tools'
'The ``<repo>/tools`` directory'
TESTS_DIR = PROJECT_ROOT / 'tests'
'The ``<repo>/tests`` directory'
BUILD_DIR = PROJECT_ROOT / '_build'
'The default build directory'
TWEAKS_DIR = PROJECT_ROOT / 'conf'
'The directory for tweak headers used by the build'
PREBUILT_DIR = PROJECT_ROOT / '_prebuilt'
'The directory were prebuild/bootstrapped results will go, and scratch space for the build'
EXE_SUFFIX = '.exe' if os.name == 'nt' else ''
'The suffix of executable files on this system'
PREBUILT_DDS = (PREBUILT_DIR / 'dds').with_suffix(EXE_SUFFIX)
'The path to the prebuilt ``dds`` executable'
CUR_BUILT_DDS = (BUILD_DIR / 'dds').with_suffix(EXE_SUFFIX)
'The path to the main built ``dds`` executable'


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
