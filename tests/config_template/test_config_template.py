from time import sleep

from dds_ci.testing import ProjectOpener


def test_config_template(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('copy_only')
    generated_fpath = proj.build_root / '__dds/gen/info.hpp'
    assert not generated_fpath.is_file()
    proj.build()
    assert generated_fpath.is_file()

    # Check that re-running the build will not update the generated file (the
    # file's content has not changed. Re-generating it would invalidate the
    # cache and force a false-rebuild.)
    start_time = generated_fpath.stat().st_mtime
    sleep(0.1)  # Wait just long enough to register a new stamp time
    proj.build()
    new_time = generated_fpath.stat().st_mtime
    assert new_time == start_time


def test_simple_substitution(project_opener: ProjectOpener) -> None:
    simple = project_opener.open('simple')
    simple.build()
