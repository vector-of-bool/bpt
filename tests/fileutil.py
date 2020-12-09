from contextlib import contextmanager
from pathlib import Path
from typing import Iterator, Optional

import shutil


@contextmanager
def ensure_dir(dirpath: Path) -> Iterator[Path]:
    """
    Ensure that the given directory (and any parents) exist. When the context
    exists, removes any directories that were created.
    """
    dirpath = dirpath.absolute()
    if dirpath.exists():
        assert dirpath.is_dir(), f'Directory {dirpath} is a non-directory file'
        yield dirpath
        return

    # Create the directory and clean it up when we are done
    with ensure_dir(dirpath.parent):
        dirpath.mkdir()
        try:
            yield dirpath
        finally:
            shutil.rmtree(dirpath)


@contextmanager
def auto_delete(fpath: Path) -> Iterator[Path]:
    try:
        yield fpath
    finally:
        if fpath.exists():
            fpath.unlink()


@contextmanager
def set_contents(fpath: Path, content: bytes) -> Iterator[Path]:
    prev_content: Optional[bytes] = None
    if fpath.exists():
        assert fpath.is_file(), 'File {fpath} exists and is not a regular file'
        prev_content = fpath.read_bytes()

    with ensure_dir(fpath.parent):
        fpath.write_bytes(content)
        try:
            yield fpath
        finally:
            if prev_content is None:
                fpath.unlink()
            else:
                fpath.write_bytes(prev_content)
