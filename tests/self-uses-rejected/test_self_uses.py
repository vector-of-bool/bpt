from dds_ci.testing import ProjectOpener, error


def test_self_referential_uses_fails(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('bad_proj')
    with error.expect_error_marker_re(r'library-json-cyclic-dependency'):
        proj.build()


def test_self_referential_uses_cycle_fails(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('bad_proj_large_cycle')
    with error.expect_error_marker_re(r'library-json-cyclic-dependency'):
        proj.build()


def test_self_referential_uses_for_libs_fails(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('bad_proj_libs')
    with error.expect_error_marker_re(r'library-json-cyclic-dependency'):
        proj.build()
