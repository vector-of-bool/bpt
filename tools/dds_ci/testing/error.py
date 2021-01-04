"""
Test utilities for error checking
"""

from contextlib import contextmanager
from typing import Iterator
import subprocess
from pathlib import Path
import tempfile
import os


@contextmanager
def expect_error_marker(expect: str) -> Iterator[None]:
    """
    A context-manager function that should wrap a scope that causes an error
    from ``dds``.

    :param expect: The error message ID string that is expected to appear.

    The wrapped scope should raise :class:`subprocess.CalledProcessError`.

    After handling the exception, asserts that the subprocess wrote an
    error marker containing the string given in ``expect``.
    """
    tdir = Path(tempfile.mkdtemp())
    err_file = tdir / 'error'
    try:
        os.environ['DDS_WRITE_ERROR_MARKER'] = str(err_file)
        yield
        assert False, 'dds subprocess did not raise CallProcessError'
    except subprocess.CalledProcessError:
        assert err_file.exists(), \
            f'No error marker file was generated, but dds exited with an error (Expected "{expect}")'
        marker = err_file.read_text().strip()
        assert marker == expect, \
            f'dds did not produce the expected error (Expected {expect}, got {marker})'
    finally:
        os.environ.pop('DDS_WRITE_ERROR_MARKER')
