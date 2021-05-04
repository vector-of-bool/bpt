import pytest
import subprocess

from dds_ci.testing import ProjectOpener, error


def test_dependent_src_header_fails(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('bad_proj')
    with error.expect_error_marker_re(r'syntax-check-failed.*bad_src\.hpp'):
        proj.build()


def test_dependent_include_header_fails(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('bad_proj_include')
    with error.expect_error_marker_re(r'syntax-check-failed.*depends_src\.hpp'):
        proj.build()
