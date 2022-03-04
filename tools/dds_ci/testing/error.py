"""
Test utilities for error checking
"""

import shutil
from contextlib import contextmanager
from typing import ContextManager, Iterator, Callable, Pattern, Union
import subprocess
from pathlib import Path
import tempfile
import os
import re


@contextmanager
def expect_error_marker_pred(pred: Callable[[str], bool], expected: str) -> Iterator[None]:
    """
    A context-manager function that should wrap a scope that causes an error
    from ``dds``.

    :param pred: A predicate which checks if the marker passes
    :param expected: A string description of the expected marker

    The wrapped scope should raise :class:`subprocess.CalledProcessError`.

    After handling the exception, asserts that the subprocess wrote an
    error marker matching ``pred``.
    """
    tdir = Path(tempfile.mkdtemp())
    err_file = tdir / 'error'
    env_key = 'DDS_WRITE_ERROR_MARKER'
    prev = os.environ.get(env_key)
    try:
        os.environ[env_key] = str(err_file)
        yield
        assert False, 'dds subprocess did not raise CallProcessError'
    except subprocess.CalledProcessError:
        assert err_file.exists(), \
            f'No error marker file [{err_file}] was generated, but dds exited with an error (Expected "{expected}")'
        marker = err_file.read_text(encoding='utf-8').strip()
        assert pred(marker), \
            f'dds did not produce the expected error (Expected {expected}, got {marker})'
    finally:
        shutil.rmtree(tdir)
        if prev:
            os.environ[env_key] = prev
        else:
            os.environ.pop(env_key)


def expect_error_marker(expect: str) -> ContextManager[None]:
    """
    A context-manager function that should wrap a scope that causes an error
    from ``dds``.

    :param expect: A string description of the expected marker

    The wrapped scope should raise :class:`subprocess.CalledProcessError`.

    After handling the exception, asserts that the subprocess wrote an
    error marker containing the string given in ``expect``.
    """
    return expect_error_marker_pred(expect.__eq__, expect)


def expect_error_marker_re(expect: Union[str, Pattern[str]]) -> ContextManager[None]:
    """
    A context-manager function that should wrap a scope that causes an error
    from ``dds``.

    :param expect: A regular expression that the expected marker should match

    The wrapped scope should raise :class:`subprocess.CalledProcessError`.

    After handling the exception, asserts that the subprocess wrote an
    error marker matching ``expect``.
    """
    expect_: Pattern[str] = re.compile(expect)
    return expect_error_marker_pred(lambda s: (expect_.search(s) is not None), expect_.pattern)
