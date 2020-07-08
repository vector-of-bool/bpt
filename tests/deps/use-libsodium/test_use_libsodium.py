from tests import DDS

from dds_ci import proc


def test_get_build_use_libsodium(dds: DDS):
    dds.catalog_import(dds.source_root / 'catalog.json')
    tc_fname = 'gcc.tc.jsonc' if 'gcc' in dds.default_builtin_toolchain else 'msvc.tc.jsonc'
    tc = str(dds.test_dir / tc_fname)
    dds.build(toolchain=tc)
    proc.check_run(
        (dds.build_dir / 'use-libsodium').with_suffix(dds.exe_suffix))
