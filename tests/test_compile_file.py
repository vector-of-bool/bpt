import subprocess

import pytest
import time

from dds_ci.testing import Project


def test_simple_compile_file(tmp_project: Project) -> None:
    """
    Check that changing a source file will update the resulting application.
    """
    with pytest.raises(subprocess.CalledProcessError):
        tmp_project.compile_file('src/answer.cpp')
    tmp_project.write('src/answer.cpp', 'int get_answer() { return 42; }')
    # No error:
    tmp_project.compile_file('src/answer.cpp')
    # Fail:
    time.sleep(1)  # Sleep long enough to register a file change
    tmp_project.write('src/answer.cpp', 'int get_answer() { return "How many roads must a man walk down?"; }')
    with pytest.raises(subprocess.CalledProcessError):
        tmp_project.compile_file('src/answer.cpp')
