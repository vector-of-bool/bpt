from tests import DDS
from tests.fileutil import ensure_dir


def test_empty_dir(dds: DDS):
    with ensure_dir(dds.source_root):
        dds.build()
