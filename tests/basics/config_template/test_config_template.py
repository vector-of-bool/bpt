from time import sleep

from tests import DDS, dds_fixture_conf_1


@dds_fixture_conf_1('copy_only')
def test_config_template(dds: DDS) -> None:
    generated_fpath = dds.build_dir / '__dds/gen/info.hpp'
    assert not generated_fpath.is_file()
    dds.build()
    assert generated_fpath.is_file()

    # Check that re-running the build will not update the generated file (the
    # file's content has not changed. Re-generating it would invalidate the
    # cache and force a false-rebuild.)
    start_time = generated_fpath.stat().st_mtime
    sleep(0.1)  # Wait just long enough to register a new stamp time
    dds.build()
    new_time = generated_fpath.stat().st_mtime
    assert new_time == start_time


@dds_fixture_conf_1('simple')
def test_simple_substitution(dds: DDS) -> None:
    dds.build()
