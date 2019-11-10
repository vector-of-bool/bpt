from contextlib import ExitStack
from tests import DDS
from tests.fileutil import set_contents


def test_simple_lib(dds: DDS, scope: ExitStack):
    scope.enter_context(
        dds.set_contents(
            'src/foo.cpp',
            b'int the_answer() { return 42; }',
        ))

    scope.enter_context(
        dds.set_contents(
            'library.dds',
            b'Name: TestLibrary',
        ))

    scope.enter_context(
        dds.set_contents(
            'package.dds',
            b'''
            Name: TestProject
            Version: 0.0.0
            ''',
        ))

    dds.build(tests=True, apps=False, warnings=False, export=True)
    assert (dds.build_dir / 'compile_commands.json').is_file()
    assert list(dds.build_dir.glob('libTestLibrary*')) != []
