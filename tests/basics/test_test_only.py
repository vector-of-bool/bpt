from contextlib import ExitStack
from tests import DDS
from tests.fileutil import set_contents

from dds_ci import paths


def test_lib_with_just_test(dds: DDS, scope: ExitStack) -> None:
    scope.enter_context(set_contents(
        dds.source_root / 'src/foo.test.cpp',
        b'int main() {}',
    ))

    dds.build(tests=True, apps=False, warnings=False)
    assert (dds.build_dir / f'test/foo{paths.EXE_SUFFIX}').is_file()
