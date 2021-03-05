import pytest
import subprocess
import shutil
from pathlib import Path
from typing import Callable

from dds_ci.testing import RepoServer, ProjectOpener


@pytest.fixture(autouse=True)
def _init_repo_catalog(test_parent_dir: Path, tmp_git_repo_factory: Callable[[str], Path], http_repo: RepoServer):
    CATALOG = {
        "version": 2,
        "packages": {
            'the_test_dependency': {
                "0.0.0": {
                    "remote": {
                        "git": {
                            "url": str(tmp_git_repo_factory(test_parent_dir / 'the_test_dependency')),
                            "ref": "tmp_git_repo"
                        },
                        "auto-lib": "dds/the_test_dependency"
                    }
                }
            },
            'the_test_lib': {
                "0.0.0": {
                    "remote": {
                        "git": {
                            "url": str(tmp_git_repo_factory(test_parent_dir / 'the_test_lib')),
                            "ref": "tmp_git_repo"
                        },
                        "auto-lib": "dds/the_test_lib"
                    }
                }
            }
        }
    }
    http_repo.import_json_data(CATALOG)

    return http_repo


def test_build_lib_with_test_uses(project_opener: ProjectOpener, http_repo: RepoServer) -> None:
    proj = project_opener.open('the_test_lib')
    proj.dds.repo_add(http_repo.url)
    proj.build()


def test_build_dependent_of_lib_with_test_uses(test_parent_dir: Path, project_opener: ProjectOpener, http_repo: RepoServer) -> None:
    proj = project_opener.open('dependents/basic_dependent')
    shutil.copytree(test_parent_dir / 'dependents/src',
                    proj.root / 'src')
    shutil.copy(test_parent_dir / 'dependents/dependent_has_dep.test.cpp',
                proj.root / 'src')
    proj.dds.repo_add(http_repo.url)
    proj.build()


def test_build_test_dependent_of_lib_with_test_uses(test_parent_dir: Path, project_opener: ProjectOpener, http_repo: RepoServer) -> None:
    proj = project_opener.open('dependents/basic_test_dependent')
    shutil.copytree(test_parent_dir / 'dependents/src',
                    proj.root / 'src')
    shutil.copy(test_parent_dir / 'dependents/dependent_no_dep.test.cpp',
                proj.root / 'src')
    proj.dds.repo_add(http_repo.url)
    proj.build()


def test_build_dependent_with_multiple_libs(test_parent_dir: Path, project_opener: ProjectOpener, http_repo: RepoServer) -> None:
    proj = project_opener.open('dependents/multi_lib_dependent')
    shutil.copytree(test_parent_dir / 'dependents/src',
                    proj.root / 'libs/uses/src/uses')
    shutil.copy(test_parent_dir / 'dependents/dependent_has_dep.test.cpp',
                proj.root / 'libs/uses/src')
    shutil.copytree(test_parent_dir / 'dependents/src',
                    proj.root / 'libs/uses_for_tests/src/test_uses')
    shutil.copy(test_parent_dir / 'dependents/dependent_no_dep.test.cpp',
                proj.root / 'libs/uses_for_tests/src')
    proj.dds.repo_add(http_repo.url)
    proj.build()


def test_fail_to_build_when_test_depends_but_regular_uses(test_parent_dir: Path, project_opener: ProjectOpener, http_repo: RepoServer) -> None:
    proj = project_opener.open('dependents/test_depends_but_regular_uses')
    shutil.copytree(test_parent_dir / 'dependents/src',
                    proj.root / 'src')
    proj.dds.repo_add(http_repo.url)
    with pytest.raises(subprocess.CalledProcessError, match=r'.*(test_depends.*test_uses|test_uses.*test_depends).*'):
        proj.build()
