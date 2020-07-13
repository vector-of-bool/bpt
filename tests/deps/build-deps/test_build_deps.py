from tests import dds, DDS


def test_build_deps_from_file(dds: DDS):
    assert not dds.deps_build_dir.is_dir()
    dds.catalog_import(dds.source_root / 'catalog.json')
    dds.build_deps(['-d', 'deps.json5'])
    assert (dds.deps_build_dir / 'neo-fun@0.3.0').is_dir()
    assert (dds.scratch_dir / 'INDEX.lmi').is_file()
    assert (dds.deps_build_dir / '_libman/neo-fun.lmp').is_file()
    assert (dds.deps_build_dir / '_libman/neo/fun.lml').is_file()


def test_build_deps_from_cmd(dds: DDS):
    assert not dds.deps_build_dir.is_dir()
    dds.catalog_import(dds.source_root / 'catalog.json')
    dds.build_deps(['neo-fun=0.3.0'])
    assert (dds.deps_build_dir / 'neo-fun@0.3.0').is_dir()
    assert (dds.scratch_dir / 'INDEX.lmi').is_file()
    assert (dds.deps_build_dir / '_libman/neo-fun.lmp').is_file()
    assert (dds.deps_build_dir / '_libman/neo/fun.lml').is_file()


def test_multiple_deps(dds: DDS):
    assert not dds.deps_build_dir.is_dir()
    dds.catalog_import(dds.source_root / 'catalog.json')
    dds.build_deps(['neo-fun^0.2.0', 'neo-fun~0.3.0'])
    assert (dds.deps_build_dir / 'neo-fun@0.3.0').is_dir()
    assert (dds.scratch_dir / 'INDEX.lmi').is_file()
    assert (dds.deps_build_dir / '_libman/neo-fun.lmp').is_file()
    assert (dds.deps_build_dir / '_libman/neo/fun.lml').is_file()
