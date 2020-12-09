from typing import ContextManager
from pathlib import Path
from tests import DDS
from tests.fileutil import ensure_dir, set_contents


def test_build_empty(dds: DDS) -> None:
    assert not dds.source_root.exists()
    dds.scope.enter_context(ensure_dir(dds.source_root))
    dds.build()


def test_build_simple(dds: DDS) -> None:
    dds.scope.enter_context(set_contents(dds.source_root / 'src/f.cpp', b'void foo() {}'))
    dds.build()


def basic_pkg_dds(dds: DDS) -> ContextManager[Path]:
    return set_contents(
        dds.source_root / 'package.json5', b'''
        {
            name: 'test-pkg',
            version: '0.2.2',
            namespace: 'test',
        }
    ''')


def test_empty_with_pkg_dds(dds: DDS) -> None:
    dds.scope.enter_context(basic_pkg_dds(dds))
    dds.build()


def test_empty_with_lib_dds(dds: DDS) -> None:
    dds.scope.enter_context(basic_pkg_dds(dds))
    dds.build()


def test_empty_sdist_create(dds: DDS) -> None:
    dds.scope.enter_context(basic_pkg_dds(dds))
    dds.sdist_create()


def test_empty_sdist_export(dds: DDS) -> None:
    dds.scope.enter_context(basic_pkg_dds(dds))
    dds.sdist_export()
