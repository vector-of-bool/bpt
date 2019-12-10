from tests import dds, DDS


def test_build_deps(dds: DDS):
    assert not dds.deps_build_dir.is_dir()
    dds.build_deps(['-d', 'deps.dds'])
    assert dds.deps_build_dir.is_dir()
