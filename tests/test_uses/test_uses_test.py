import pytest
import shutil
from pathlib import Path

from dds_ci.testing import RepoServer, ProjectOpener, error, CRSRepo, CRSRepoFactory
from dds_ci.testing.fixtures import TmpGitRepoFactory, FixtureRequest


@pytest.fixture(scope='module')
def ut_repo(crs_repo_factory: CRSRepoFactory, test_parent_dir: Path) -> CRSRepo:
    repo = crs_repo_factory('uses-test')
    repo.import_dir(test_parent_dir / 'the_test_dependency')
    repo.import_dir(test_parent_dir / 'the_test_lib')
    repo.import_dir(test_parent_dir / 'unbuildable')
    repo.import_dir(test_parent_dir / 'with_bad_test_dep')
    return repo


## TODO: Do not build test libs if we are not building tests
# def test_build_lib_with_failing_test_dep(project_opener: ProjectOpener, ut_repo: CRSRepo) -> None:
#     proj = project_opener.open('with_bad_test_dep')
#     # If we disable tests, then we won't try to build the test libraries, and
#     # therefore won't hit the compilation error
#     proj.build(with_tests=False, repos=[ut_repo.path])
#     # Compiling with tests enabled produces an error
#     with error.expect_error_marker('compile-failed'):
#         proj.build(with_tests=True)
#     # Check that nothing changed spuriously
#     proj.build(with_tests=False)


def test_build_lib_with_failing_transitive_test_dep(project_opener: ProjectOpener, ut_repo: CRSRepo) -> None:
    proj = project_opener.open('with_transitive_bad_test_dep')
    # Even though the transitive dependency has a test-dependency that fails, we
    # won't ever try to build that dependency ourselves, so we're okay.
    proj.build(with_tests=True, repos=[ut_repo.path])
    proj.build(with_tests=False, repos=[ut_repo.path])
    proj.build(with_tests=True, repos=[ut_repo.path])
    proj.build(with_tests=False, repos=[ut_repo.path])


def test_build_lib_with_test_uses(project_opener: ProjectOpener, ut_repo: CRSRepo) -> None:
    proj = project_opener.open('the_test_lib')
    proj.build(repos=[ut_repo.path])


def test_build_dependent_of_lib_with_test_uses(test_parent_dir: Path, project_opener: ProjectOpener,
                                               ut_repo: CRSRepo) -> None:
    proj = project_opener.open('dependents/basic_dependent')
    shutil.copytree(test_parent_dir / 'dependents/src', proj.root / 'src')
    shutil.copy(test_parent_dir / 'dependents/dependent_has_dep.test.cpp', proj.root / 'src')
    proj.build(repos=[ut_repo.path])


def test_build_test_dependent_of_lib_with_test_uses(test_parent_dir: Path, project_opener: ProjectOpener,
                                                    ut_repo: CRSRepo) -> None:
    proj = project_opener.open('dependents/basic_test_dependent')
    shutil.copytree(test_parent_dir / 'dependents/src', proj.root / 'src')
    shutil.copy(test_parent_dir / 'dependents/dependent_no_dep.test.cpp', proj.root / 'src')
    proj.build(repos=[ut_repo.path])


def test_build_dependent_with_multiple_libs(test_parent_dir: Path, project_opener: ProjectOpener,
                                            ut_repo: CRSRepo) -> None:
    proj = project_opener.open('dependents/multi_lib_dependent')
    shutil.copytree(test_parent_dir / 'dependents/src', proj.root / 'libs/uses/src/uses')
    shutil.copy(test_parent_dir / 'dependents/dependent_has_dep.test.cpp', proj.root / 'libs/uses/src')
    shutil.copytree(test_parent_dir / 'dependents/src', proj.root / 'libs/uses_for_tests/src/test_uses')
    shutil.copy(test_parent_dir / 'dependents/dependent_no_dep.test.cpp', proj.root / 'libs/uses_for_tests/src')
    proj.build(repos=[ut_repo.path])
