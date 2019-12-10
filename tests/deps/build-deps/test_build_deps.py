from tests import dds, DDS


def test_build_deps_from_file(dds: DDS):
    assert not dds.deps_build_dir.is_dir()
    dds.catalog_import(dds.source_root / 'catalog.json')
    dds.build_deps(['-d', 'deps.dds'])
    assert (dds.deps_build_dir / 'neo-sqlite3@0.1.0').is_dir()
    assert (dds.scratch_dir / 'INDEX.lmi').is_file()
    assert (dds.deps_build_dir / '_libman/neo-sqlite3.lmp').is_file()
    assert (dds.deps_build_dir / '_libman/neo/sqlite3.lml').is_file()


def test_build_deps_from_cmd(dds: DDS):
    assert not dds.deps_build_dir.is_dir()
    dds.catalog_import(dds.source_root / 'catalog.json')
    dds.build_deps(['neo-sqlite3 =0.1.0'])
    assert (dds.deps_build_dir / 'neo-sqlite3@0.1.0').is_dir()
    assert (dds.scratch_dir / 'INDEX.lmi').is_file()
    assert (dds.deps_build_dir / '_libman/neo-sqlite3.lmp').is_file()
    assert (dds.deps_build_dir / '_libman/neo/sqlite3.lml').is_file()


def test_multiple_deps(dds: DDS):
    assert not dds.deps_build_dir.is_dir()
    dds.catalog_import(dds.source_root / 'catalog.json')
    dds.build_deps(['neo-sqlite3 ^0.1.0', 'neo-sqlite3 ~0.2.0'])
    assert (dds.deps_build_dir / 'neo-sqlite3@0.2.2').is_dir()
    assert (dds.scratch_dir / 'INDEX.lmi').is_file()
    assert (dds.deps_build_dir / '_libman/neo-sqlite3.lmp').is_file()
    assert (dds.deps_build_dir / '_libman/neo/sqlite3.lml').is_file()
