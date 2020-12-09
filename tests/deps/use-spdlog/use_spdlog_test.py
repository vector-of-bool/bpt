from tests import DDS
from tests.http import RepoFixture

from dds_ci import proc, paths, toolchain


def test_get_build_use_spdlog(dds: DDS, http_repo: RepoFixture) -> None:
    http_repo.import_json_file(dds.source_root / 'catalog.json')
    dds.repo_add(http_repo.url)
    tc_fname = 'gcc.tc.jsonc' if 'gcc' in toolchain.get_default_test_toolchain().name else 'msvc.tc.jsonc'
    tc = str(dds.test_dir / tc_fname)
    dds.build(toolchain=tc, apps=True)
    proc.check_run([(dds.build_dir / 'use-spdlog').with_suffix(paths.EXE_SUFFIX)])
