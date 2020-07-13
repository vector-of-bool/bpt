from tests import DDS
import platform

import pytest

from dds_ci import proc


@pytest.mark.skipif(
    platform.system() == 'FreeBSD',
    reason='This one has trouble running on FreeBSD')
def test_get_build_use_cryptopp(dds: DDS):
    dds.catalog_import(dds.source_root / 'catalog.json')
    tc_fname = 'gcc.tc.jsonc' if 'gcc' in dds.default_builtin_toolchain else 'msvc.tc.jsonc'
    tc = str(dds.test_dir / tc_fname)
    dds.build(toolchain=tc)
    proc.check_run(
        (dds.build_dir / 'use-cryptopp').with_suffix(dds.exe_suffix))
