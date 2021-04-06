import pytest
from pathlib import Path

from dds_ci.testing import RepoServer, ProjectOpener, error
from dds_ci import proc, paths, toolchain


@pytest.fixture()
def repo_with_spdlog(http_repo: RepoServer, test_parent_dir: Path) -> RepoServer:
    catalog = test_parent_dir / 'project/catalog.json'
    http_repo.import_json_file(catalog)
    return http_repo


@pytest.fixture()
def toolchain_path(test_parent_dir: Path) -> Path:
    tc_fname = 'gcc.tc.jsonc' if 'gcc' in toolchain.get_default_test_toolchain().name else 'msvc.tc.jsonc'
    return test_parent_dir / tc_fname


def test_get_build_use_spdlog(project_opener: ProjectOpener, toolchain_path: Path,
                              repo_with_spdlog: RepoServer) -> None:
    proj = project_opener.open('project')
    proj.dds.repo_add(repo_with_spdlog.url)
    proj.build(toolchain=toolchain_path)
    proc.check_run([(proj.build_root / 'use-spdlog').with_suffix(paths.EXE_SUFFIX)])


def test_invalid_uses_specifier(project_opener: ProjectOpener, toolchain_path: Path,
                                repo_with_spdlog: RepoServer) -> None:
    proj = project_opener.open('project')
    lib_json = proj.library_json
    lib_json['uses'] = ['not-valid']
    proj.library_json = lib_json
    proj.dds.repo_add(repo_with_spdlog.url)
    with error.expect_error_marker('invalid-uses-spec'):
        proj.build(toolchain=toolchain_path)
