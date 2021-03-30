import pytest
import subprocess

from dds_ci.testing import ProjectOpener


def test_dependent_src_header_fails(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('bad_proj')
    with pytest.raises(subprocess.CalledProcessError,
                       match=r'bad_src.hpp.*?failed to compile in isolation'):
        proj.build()


def test_dependent_include_header_fails(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('bad_proj')
    with pytest.raises(subprocess.CalledProcessError,
                       match=r'depends_src.hpp.*?failed to compile in isolation'):
        proj.build()
