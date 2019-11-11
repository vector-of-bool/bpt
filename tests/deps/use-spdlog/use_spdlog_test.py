from tests import DDS

from dds_ci import proc


def test_get_build_use_spdlog(dds: DDS):
    dds.deps_get()
    tc_fname = 'gcc.tc.dds' if 'gcc' in dds.default_builtin_toolchain else 'msvc.tc.dds'
    tc = str(dds.test_dir / tc_fname)
    dds.deps_build(toolchain=tc)
    dds.build(toolchain=tc, apps=True)
    proc.check_run((dds.build_dir / 'use-spdlog').with_suffix(dds.exe_suffix))
