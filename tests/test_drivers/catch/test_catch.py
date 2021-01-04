from dds_ci import proc, paths
from dds_ci.testing import ProjectOpener


def test_main(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('main')
    proj.build()
    test_exe = proj.build_root.joinpath('test/testlib/calc' + paths.EXE_SUFFIX)
    assert test_exe.is_file()
    assert proc.run([test_exe]).returncode == 0


def test_custom(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('custom-runner')
    proj.build()
    test_exe = proj.build_root.joinpath('test/testlib/calc' + paths.EXE_SUFFIX)
    assert test_exe.is_file()
    assert proc.run([test_exe]).returncode == 0
