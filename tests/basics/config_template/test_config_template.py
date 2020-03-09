import pytest

from tests import DDS, dds_fixture_conf_1


@dds_fixture_conf_1('copy_only')
def test_config_template(dds: DDS):
    dds.build()
    assert (dds.build_dir / '__dds/gen/info.hpp').is_file()
