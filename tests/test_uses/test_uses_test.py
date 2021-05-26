import pytest
import subprocess
import shutil
from pathlib import Path
from typing import Callable

from dds_ci.testing import RepoServer, ProjectOpener, error


@pytest.fixture(autouse=True)
def _init_repo(tmp_git_repo_factory: Callable[[str], Path], module_http_repo: RepoServer) -> RepoServer:
    parent_dir = Path(__file__).parent.resolve()
    # yapf: disable
    CATALOG = {
        "version": 2,
        "packages": {
            'the_test_dependency': {
                "0.0.0": {
                    "remote": {
                        "git": {
                            "url": str(tmp_git_repo_factory(parent_dir / 'the_test_dependency')),
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
                            "url": str(tmp_git_repo_factory(parent_dir / 'the_test_lib')),
                            "ref": "tmp_git_repo"
                        },
                        "auto-lib": "dds/the_test_lib"
                    }
                }
            },
            'unbuildable': {
                "0.0.0": {
                    "remote": {
                        "git": {
                            "url": str(tmp_git_repo_factory(parent_dir / 'unbuildable')),
                            "ref": "tmp_git_repo"
                        },
                    }
                }
            },
            'with_bad_test_dep': {
                "0.0.0": {
                    "remote": {
                        "git": {
                            "url": str(tmp_git_repo_factory(parent_dir / 'with_bad_test_dep')),
                            "ref": "tmp_git_repo"
                        },
                    }
                }
            }
        }
    }
    # yapf: enable
    module_http_repo.import_json_data(CATALOG)

    return module_http_repo


def test_build_lib_with_failing_test_dep(project_opener: ProjectOpener, module_http_repo: RepoServer) -> None:
    proj = project_opener.open('with_bad_test_dep')
    proj.dds.repo_add(module_http_repo.url)
    # If we disable tests, then we won't try to build the test libraries, and
    # therefore won't hit the compilation error
    proj.build(with_tests=False)
    # Compiling with tests enabled produces an error
    with error.expect_error_marker('compile-failed'):
        proj.build(with_tests=True)
    # Check that nothing changed spuriously
    proj.build(with_tests=False)


def test_build_lib_with_failing_transitive_test_dep(project_opener: ProjectOpener, module_http_repo: RepoServer) -> None:
    proj = project_opener.open('with_transitive_bad_test_dep')
    proj.dds.repo_add(module_http_repo.url)
    # Even though the transitive dependency has a test-dependency that fails, we
    # won't ever try to build that dependency ourselves, so we're okay.
    proj.build(with_tests=True)
    proj.build(with_tests=False)
    proj.build(with_tests=True)
    proj.build(with_tests=False)


def test_build_lib_with_test_uses(project_opener: ProjectOpener, module_http_repo: RepoServer) -> None:
    proj = project_opener.open('the_test_lib')
    proj.dds.repo_add(module_http_repo.url)
    proj.build()


def test_build_dependent_of_lib_with_test_uses(test_parent_dir: Path, project_opener: ProjectOpener,
                                               module_http_repo: RepoServer) -> None:
    proj = project_opener.open('dependents/basic_dependent')
    shutil.copytree(test_parent_dir / 'dependents/src', proj.root / 'src')
    shutil.copy(test_parent_dir / 'dependents/dependent_has_dep.test.cpp', proj.root / 'src')
    proj.dds.repo_add(module_http_repo.url)
    proj.build()


def test_build_test_dependent_of_lib_with_test_uses(test_parent_dir: Path, project_opener: ProjectOpener,
                                                    module_http_repo: RepoServer) -> None:
    proj = project_opener.open('dependents/basic_test_dependent')
    shutil.copytree(test_parent_dir / 'dependents/src', proj.root / 'src')
    shutil.copy(test_parent_dir / 'dependents/dependent_no_dep.test.cpp', proj.root / 'src')
    proj.dds.repo_add(module_http_repo.url)
    proj.build()


def test_build_dependent_with_multiple_libs(test_parent_dir: Path, project_opener: ProjectOpener,
                                            module_http_repo: RepoServer) -> None:
    proj = project_opener.open('dependents/multi_lib_dependent')
    shutil.copytree(test_parent_dir / 'dependents/src', proj.root / 'libs/uses/src/uses')
    shutil.copy(test_parent_dir / 'dependents/dependent_has_dep.test.cpp', proj.root / 'libs/uses/src')
    shutil.copytree(test_parent_dir / 'dependents/src', proj.root / 'libs/uses_for_tests/src/test_uses')
    shutil.copy(test_parent_dir / 'dependents/dependent_no_dep.test.cpp', proj.root / 'libs/uses_for_tests/src')
    proj.dds.repo_add(module_http_repo.url)
    proj.build()
