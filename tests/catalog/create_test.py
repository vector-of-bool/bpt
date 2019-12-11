from tests import dds, DDS
from tests.fileutil import ensure_dir


def test_create_catalog(dds: DDS):
    dds.scope.enter_context(ensure_dir(dds.build_dir))
    dds.catalog_create()
    assert dds.catalog_path.is_file()
