from dds_ci.testing import ProjectOpener


def test_compile_libs_with_colliding_object_files(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('proj')
    proj.build()
