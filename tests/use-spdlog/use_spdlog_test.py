import json
import pytest
from pathlib import Path

from dds_ci.testing import ProjectOpener, error, CRSRepo, CRSRepoFactory
from dds_ci import proc, paths, toolchain
from dds_ci.testing.fs import DirRenderer, render_into
from dds_ci.testing.repo import make_simple_crs


@pytest.fixture(scope='session')
def spdlog_v1_4_2(dir_renderer: DirRenderer) -> Path:
    with dir_renderer.get_or_prepare('spdlog-v1.4.2-v1') as prep:
        if prep.ready_path:
            return prep.ready_path
        assert prep.prep_path
        proc.check_run(
            ['git', 'clone', 'https://github.com/gabime/spdlog.git', '--branch=v1.4.2', '--depth=1', prep.prep_path])
        render_into(prep.prep_path, {'pkg.json': json.dumps(make_simple_crs('spdlog', '1.4.2'))})
        return prep.commit()


@pytest.fixture(scope='module')
def repo_with_spdlog(crs_repo_factory: CRSRepoFactory, spdlog_v1_4_2: Path) -> CRSRepo:
    repo = crs_repo_factory('with-spdlog')
    repo.import_(spdlog_v1_4_2)
    return repo


@pytest.fixture()
def toolchain_path(test_parent_dir: Path) -> Path:
    tc_fname = 'gcc.tc.jsonc' if 'gcc' in toolchain.get_default_test_toolchain().name else 'msvc.tc.jsonc'
    return test_parent_dir / tc_fname


def test_get_build_use_spdlog(project_opener: ProjectOpener, toolchain_path: Path, repo_with_spdlog: CRSRepo) -> None:
    proj = project_opener.open('project')
    proj.build(toolchain=toolchain_path, repos=[repo_with_spdlog.path])
    proc.check_run([(proj.build_root / 'use-spdlog').with_suffix(paths.EXE_SUFFIX)])


def test_invalid_uses_specifier(project_opener: ProjectOpener, toolchain_path: Path, repo_with_spdlog: CRSRepo) -> None:
    proj = project_opener.open('project')
    proj.project_json = {
        'name': 'test',
        'version': '0.0.0',
        'depends': ['spdlog@1.4.2 uses no-such-library'],
    }
    with error.expect_error_marker('no-dependency-solution'):
        proj.build(toolchain=toolchain_path, repos=[repo_with_spdlog.path])
