import pytest
import shutil
from pathlib import Path

from bpt_ci.testing import ProjectOpener, CRSRepo, CRSRepoFactory, error, fs
from bpt_ci.testing.fixtures import Project


@pytest.fixture(scope='module')
def ut_repo(crs_repo_factory: CRSRepoFactory, test_parent_dir: Path) -> CRSRepo:
    repo = crs_repo_factory('uses-test')
    names = ('the_test_dependency', 'the_test_lib', 'unbuildable', 'with_bad_test_dep', 'partially_buildable')
    repo.import_((test_parent_dir / name for name in names))
    return repo


def test_build_lib_with_failing_test_dep(project_opener: ProjectOpener, ut_repo: CRSRepo) -> None:
    proj = project_opener.open('with_bad_test_dep')
    # If we disable tests, then we won't try to build the test libraries, and
    # therefore won't hit the compilation error
    proj.build(with_tests=False, repos=[ut_repo.path])
    # Compiling with tests enabled produces an error
    with error.expect_error_marker('compile-failed'):
        proj.build(with_tests=True, repos=[ut_repo.path])
    # Check that nothing changed spuriously
    proj.build(with_tests=False, repos=[ut_repo.path])


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


def test_uses_sibling_lib(tmp_project: Project) -> None:
    fs.render_into(
        tmp_project.root, {
            'main': {
                'src': {
                    'foo.cpp': b'int number() { return 42; }\n',
                    'foo.h': 'extern int number();\n',
                }
            },
            'there': {
                'src': {
                    't.test.cpp':
                    r'''
                        #include <foo.h>
                        #include <cassert>

                        int main () { assert(number() == 42); }
                    '''
                }
            }
        })

    # Missing the 'using'
    tmp_project.bpt_yaml = {
        'name': 'testing',
        'version': '1.2.3',
        'libraries': [{
            'name': 'main',
            'path': 'main',
        }, {
            'name': 'other',
            'path': 'there',
        }]
    }
    with error.expect_error_marker('compile-failed'):
        tmp_project.build()

    # Now add the missing 'using'
    tmp_project.bpt_yaml = {
        'name': 'testing',
        'version': '1.2.3',
        'libraries': [{
            'name': 'main',
            'path': 'main',
        }, {
            'name': 'other',
            'path': 'there',
            'using': ['main']
        }]
    }
    tmp_project.build()


def test_build_partial_dep(tmp_project: Project, ut_repo: CRSRepo) -> None:
    fs.render_into(
        tmp_project.root, {
            'src': {
                'use-lib.cpp':
                r'''
                #include <can_build/header.hpp>

                int main() {
                    return CAN_BUILD_ZERO;
                }
                ''',
            },
        })

    tmp_project.bpt_yaml = {
        'name': 'test-user',
        'version': '1.2.3',
        'dependencies': ['partially_buildable@0.1.0 using can_build'],
    }
    tmp_project.build(repos=[ut_repo.path])

    tmp_project.bpt_yaml['dependencies'] = ['partially_buildable@0.1.0 using can_build, cannot_build']
    with error.expect_error_marker('compile-failed'):
        tmp_project.build(repos=[ut_repo.path])
