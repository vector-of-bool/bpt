import subprocess

import pytest

from tests import DDS
from dds_ci import proc, paths

## #############################################################################
## #############################################################################
## The test project in this directory contains a single application and two
## functions, each defined in a separate file. The two functions each return
## an integer, and the application exit code will be the difference between
## the two integers. (They are passed through std::abs(), so it is always a
## positive integer). The default value is 32 in both functions.
## #############################################################################
## The purpose of these tests is to ensure the reliability of the compilation
## dependency database. Having a miscompile because there was a failure to
## detect file changes is a catastrophic bug!


def build_and_get_rc(dds: DDS) -> int:
    dds.build()
    app = dds.build_dir / ('app' + paths.EXE_SUFFIX)
    return proc.run([app]).returncode


def test_simple_rebuild(dds: DDS) -> None:
    """
    Check that changing a source file will update the resulting application.
    """
    assert build_and_get_rc(dds) == 0
    dds.scope.enter_context(
        dds.set_contents(
            'src/1.cpp',
            b'''
            int value_1() { return 33; }
            ''',
        ))
    # 33 - 32 = 1
    assert build_and_get_rc(dds) == 1


def test_rebuild_header_change(dds: DDS) -> None:
    """Change the content of the header which defines the values"""
    assert build_and_get_rc(dds) == 0
    dds.scope.enter_context(
        dds.set_contents(
            'src/values.hpp',
            b'''
            const int first_value = 63;
            const int second_value = 88;
            ''',
        ))
    assert build_and_get_rc(dds) == (88 - 63)


def test_partial_build_rebuild(dds: DDS) -> None:
    """
    Change the content of a header, but cause one user of that header to fail
    compilation. The fact that compilation fails means it is still `out-of-date`,
    and will need to be compiled after we have fixed it up.
    """
    assert build_and_get_rc(dds) == 0
    dds.scope.enter_context(
        dds.set_contents(
            'src/values.hpp',
            b'''
            const int first_value_q  = 6;
            const int second_value_q = 99;
            ''',
        ))
    # Header now causes errors in 1.cpp and 2.cpp
    with pytest.raises(subprocess.CalledProcessError):
        dds.build()
    # Fix 1.cpp
    dds.scope.enter_context(
        dds.set_contents(
            'src/1.cpp',
            b'''
            #include "./values.hpp"

            int value_1() { return first_value_q; }
            ''',
        ))
    # We will still see a failure, but now the DB will record the updated values.hpp
    with pytest.raises(subprocess.CalledProcessError):
        dds.build()

    # Should should raise _again_, even though we've successfully compiled one
    # of the two files with the changed `values.hpp`, because `2.cpp` still
    # has a pending update
    with pytest.raises(subprocess.CalledProcessError):
        dds.build()

    dds.scope.enter_context(
        dds.set_contents(
            'src/2.cpp',
            b'''
            #include "./values.hpp"

            int value_2() { return second_value_q; }
            ''',
        ))
    # We should now compile and link to get the updated value
    assert build_and_get_rc(dds) == (99 - 6)
