from pathlib import Path

from dds_ci.testing import RepoFixture, ProjectOpener
from dds_ci import proc, paths, toolchain


def test_get_build_use_spdlog(test_parent_dir: Path, project_opener: ProjectOpener, http_repo: RepoFixture) -> None:
    proj = project_opener.open('project')
    http_repo.import_json_file(proj.root / 'catalog.json')
    proj.dds.repo_add(http_repo.url)
    tc_fname = 'gcc.tc.jsonc' if 'gcc' in toolchain.get_default_test_toolchain().name else 'msvc.tc.jsonc'
    proj.build(toolchain=test_parent_dir / tc_fname)
    proc.check_run([(proj.build_root / 'use-spdlog').with_suffix(paths.EXE_SUFFIX)])
