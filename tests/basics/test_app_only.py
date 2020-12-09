from tests import DDS
from tests.fileutil import set_contents

from dds_ci import paths


def test_lib_with_just_app(dds: DDS) -> None:
    dds.scope.enter_context(set_contents(
        dds.source_root / 'src/foo.main.cpp',
        b'int main() {}',
    ))

    dds.build()
    assert (dds.build_dir / f'foo{paths.EXE_SUFFIX}').is_file()
