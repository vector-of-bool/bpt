from contextlib import ExitStack
from tests import DDS


def test_simple_lib(dds: DDS, scope: ExitStack) -> None:
    scope.enter_context(dds.set_contents(
        'src/foo.cpp',
        b'int the_answer() { return 42; }',
    ))

    scope.enter_context(dds.set_contents(
        'library.json5',
        b'''{
                name: 'TestLibrary',
            }''',
    ))

    scope.enter_context(
        dds.set_contents(
            'package.json5',
            b'''{
                name: 'TestProject',
                version: '0.0.0',
                namespace: 'test',
            }''',
        ))

    dds.build(tests=True, apps=False, warnings=False)
    assert (dds.build_dir / 'compile_commands.json').is_file()
    assert list(dds.build_dir.glob('libTestLibrary*')) != []
