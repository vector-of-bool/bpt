from tests.dds import DDS, dds_fixture_conf_1

@dds_fixture_conf_1('create')
def test_create_sdist(dds: DDS):
    dds.sdist_create()
    sd_dir = dds.build_dir / 'created-sdist.sds'
    assert sd_dir.is_dir()
    foo_cpp = sd_dir / 'src/foo.cpp'
    assert foo_cpp.is_file()
    header_hpp = sd_dir / 'include/header.hpp'
    assert header_hpp.is_file()
    header_h = sd_dir / 'include/header.h'
    assert header_h.is_file()

    dds.sdist_export()
    assert (dds.repo_dir / 'foo@1.2.3').is_dir()
