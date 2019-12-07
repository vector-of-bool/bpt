from contextlib import ExitStack
from tests import DDS
from tests.fileutil import set_contents


def test_lib_with_just_app(dds: DDS):
    dds.scope.enter_context(
        set_contents(
            dds.source_root / 'src/foo.main.cpp',
            b'int main() {}',
        ))

    dds.build()
    assert (dds.build_dir / f'foo{dds.exe_suffix}').is_file()
