from dds_ci.testing import ProjectOpener, error


def test_self_referential_uses_fails(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('bad_proj')
    with error.expect_error_marker_re(r'library-json-cyclic-dependency'):
        proj.build()


def test_self_referential_test_uses_fails(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('bad_proj_test_uses')
    with error.expect_error_marker_re(r'library-json-cyclic-dependency'):
        proj.build()
