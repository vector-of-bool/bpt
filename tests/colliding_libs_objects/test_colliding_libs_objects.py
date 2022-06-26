from bpt_ci.testing import ProjectOpener
from bpt_ci.testing.error import expect_error_marker


def test_compile_libs_with_colliding_object_files(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('proj')
    proj.build()


def test_compile_libs_with_colliding_test_executables(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('proj-exec')
    with expect_error_marker('build-failed-test-failed'):
        # Use only a single process to make the failure consistent
        proj.build(jobs=1)
