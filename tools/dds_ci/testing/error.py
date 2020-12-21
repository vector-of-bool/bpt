"""
Test utility for error checking
"""

from contextlib import contextmanager
from typing import Iterator
import subprocess
from pathlib import Path
import tempfile
import os


@contextmanager
def expect_error_marker(expect: str) -> Iterator[None]:
    tdir = Path(tempfile.mkdtemp())
    err_file = tdir / 'error'
    try:
        os.environ['DDS_WRITE_ERROR_MARKER'] = str(err_file)
        yield
        assert False, 'dds subprocess did not raise CallProcessError!'
    except subprocess.CalledProcessError:
        assert err_file.exists(), 'No error marker file was generated, but dds exited with an error'
        marker = err_file.read_text().strip()
        assert marker == expect, \
            f'dds did not produce the expected error (Expected {expect}, got {marker})'
    finally:
        os.environ.pop('DDS_WRITE_ERROR_MARKER')
