from tests import DDS
from tests.fileutil import ensure_dir


def test_empty_dir(dds: DDS) -> None:
    with ensure_dir(dds.source_root):
        dds.build()
