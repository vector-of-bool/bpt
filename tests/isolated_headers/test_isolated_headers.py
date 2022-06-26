import pytest
import subprocess

from bpt_ci.testing import ProjectOpener, error


def test_dependent_src_header_fails(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('bad_proj')
    ### XXX: Create a separate error-handling path for header-check failures and don't reuse the compile_failure error
    with error.expect_error_marker_re('compile-failed'):
        proj.build()


def test_dependent_include_header_fails(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('bad_proj_include')
    with error.expect_error_marker_re('compile-failed'):
        proj.build()


def test_dependent_inl_or_ipp_succeeds(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('good_proj_inl')
    proj.build()
