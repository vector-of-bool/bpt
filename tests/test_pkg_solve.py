from contextlib import contextmanager
from typing import Dict, NamedTuple, Sequence
import itertools
import pytest
import json

from typing_extensions import TypedDict, Protocol

from bpt_ci.testing import CRSRepo, Project, error
from bpt_ci.testing.fs import DirRenderer, TreeData
from bpt_ci.testing.repo import RepoCloner, make_simple_crs

_RepoPackageLibraryItem_Opt = TypedDict(
    '_RepoPackageLibraryItem_Opt',
    {
        'using': Sequence[str],
        'dependencies': Sequence[str],
        'content': TreeData,
    },
    total=False,
)
_RepoPackageLibraryItem_Required = TypedDict('_RepoPackageLibraryItem', {})


@contextmanager
def expect_solve_fail():
    with error.expect_error_marker('no-dependency-solution'):
        yield


class _RepoPackageLibraryItem(_RepoPackageLibraryItem_Opt, _RepoPackageLibraryItem_Required):
    pass


_RepoPackageItem = TypedDict('_RepoPackageItem', {
    'libs': Dict[str, _RepoPackageLibraryItem],
})
_RepoPackageVersionsDict = Dict[str, _RepoPackageItem]
_RepoPackagesDict = Dict[str, _RepoPackageVersionsDict]

RepoSpec = TypedDict('RepoSpec', {'packages': _RepoPackagesDict})


def _render_pkg_version(name: str, version: str, item: _RepoPackageItem) -> TreeData:
    proj = {
        'name':
        name,
        'version':
        version,
        'libs': [
            {
                'name': lib_name,
                'path': f'libs/{lib_name}',
                'using': lib.get('using', []),
                'dependencies': lib.get('dependencies', []),
            } for lib_name, lib in item['libs'].items()  #
        ]
    }
    return {
        'pkg.yaml': json.dumps(proj),
        'libs': {
            lib_name: lib.get('content', {})
            for lib_name, lib in item['libs'].items()  #
        },
    }


def _render_repospec(spec: RepoSpec) -> TreeData:
    pkgs = spec['packages']
    pkg_pairs = pkgs.items()
    triples = itertools.chain.from_iterable(  #
        ((pkg, version, vspec) for version, vspec in versions.items())  #
        for pkg, versions in pkg_pairs)
    return {f'{name}@{version}': _render_pkg_version(name, version, pkg) for name, version, pkg in triples}


class QuickRepo(NamedTuple):
    repo: CRSRepo

    def pkg_solve(self, *pkgs: str) -> None:
        bpt = self.repo.bpt.clone()
        bpt.crs_cache_dir = self.repo.path / '_bpt-cache'
        bpt.pkg_solve(repos=[self.repo.path], pkgs=pkgs)

    @property
    def path(self):
        """The path of the reqpo"""
        return self.repo.path


class QuickRepoFactory(Protocol):

    def __call__(self, name: str, spec: RepoSpec, validate: bool = True) -> QuickRepo:
        ...


@pytest.fixture(scope='session')
def make_quick_repo(dir_renderer: DirRenderer, session_empty_crs_repo: CRSRepo,
                    clone_repo: RepoCloner) -> QuickRepoFactory:

    def _make(name: str, spec: RepoSpec, validate: bool = True) -> QuickRepo:
        repo = clone_repo(session_empty_crs_repo)
        data = _render_repospec(spec)
        content = dir_renderer.get_or_render(f'{name}-pkgs', data)
        repo.import_(content.iterdir(), validate=False)
        if validate:
            repo.validate()
        return QuickRepo(repo)

    return _make


@pytest.fixture(scope='session')
def solve_repo_1(make_quick_repo: QuickRepoFactory) -> QuickRepo:
    # yapf: disable
    return make_quick_repo(
        'test1',
        validate=False,
        spec={
            'packages': {
                'foo': {
                    '1.3.1': {'libs': {'main': {'using': ['bar']}, 'bar': {}}}
                },
                'pkg-adds-library': {
                    '1.2.3': {'libs': {'main': {}}},
                    '1.2.4': {'libs': {'main': {}, 'another': {}}},
                },
                'pkg-conflicting-libset': {
                    '1.2.3': {'libs': {'main': {}}},
                    '1.2.4': {'libs': {'another': {}}},
                },
                'pkg-conflicting-libset-2': {
                    '1.2.3': {'libs': {'main': {'dependencies': ['nonesuch@1.2.3 using bleh']}}},
                    '1.2.4': {'libs': {'nope': {}}},
                },
                'pkg-transitive-no-such-lib': {
                    '1.2.3': {'libs': {'main': {'dependencies': ['foo@1.2.3 using no-such-lib']}}}
                },
                'pkg-with-unsolvable-version': {
                    '1.2.3': {'libs': {'main': {'dependencies': ['nonesuch@1.2.3 using bleh']}}},
                    '1.2.4': {'libs': {'main': {}}},
                },
                'pkg-with-unsolvable-library': {
                    '1.2.3': {'libs': {'goodlib': {}, 'badlib': {'dependencies': ['nonesuch@1.2.3 using bleh']}}},
                },

                'pkg-unsolvable-version-diamond': {
                    '1.2.3': {'libs': {'main': {'dependencies': ['ver-diamond-left=1.2.3 using main', 'ver-diamond-right=1.2.3 using main']}}},
                },
                'ver-diamond-left': {
                    '1.2.3': {'libs': {'main': {'dependencies': ['ver-diamond-tip=2.0.0 using main']}}}
                },
                'ver-diamond-right': {
                    '1.2.3': {'libs': {'main': {'dependencies': ['ver-diamond-tip=3.0.0 using main']}}}
                },
                'ver-diamond-tip': {
                    '2.0.0': {'libs': {'main': {}}},
                    '3.0.0': {'libs': {'main': {}}},
                },

                'pkg-unsolvable-lib-diamond': {
                    '1.2.3': {'libs': {'main': {'dependencies': ['lib-diamond-left=1.2.3 using main', 'lib-diamond-right=1.2.3 using main']}}},
                },
                'lib-diamond-left': {
                    '1.2.3': {'libs': {'main': {'dependencies': ['lib-diamond-tip^2.0.0 using lib-foo']}}}
                },
                'lib-diamond-right': {
                    '1.2.3': {'libs': {'main': {'dependencies': ['lib-diamond-tip^2.0.0 using lib-bar']}}}
                },
                'lib-diamond-tip': {
                    '2.0.0': {'libs': {'lib-foo': {}}},
                    '2.0.1': {'libs': {'lib-bar': {}}},
                },
            },
        },
    )
    # yapf: enable


def test_solve_1(solve_repo_1: QuickRepo) -> None:
    solve_repo_1.pkg_solve('foo@1.3.1 using main')


def test_solve_upgrade_pkg_version(make_quick_repo: QuickRepoFactory, tmp_project: Project,
                                   dir_renderer: DirRenderer) -> None:
    '''
    Test that bpt will pull a new copy of a package if its pkg_version is updated,
    even if the version proper is not changed.
    '''
    # yapf: disable
    repo = make_quick_repo(
        name='upgrade-pkg-revision',
        spec={
            'packages': {
                'foo': {
                    '1.2.3': {
                        'libs': {
                            'main': {
                                'content': {
                                    'src': {
                                        'foo.hpp': '#error This file is bad\n'
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    )
    # yapf: enable
    tmp_project.pkg_yaml = {'name': 'test-proj', 'version': '1.2.3', 'dependencies': ['foo@1.2.3 using main']}
    tmp_project.write('src/file.cpp', '#include <foo.hpp>\n')
    with error.expect_error_marker('compile-failed'):
        tmp_project.build(repos=[repo.path])

    # Create a new package that is good
    new_foo_crs = make_simple_crs('foo', '1.2.3', pkg_version=2)
    new_foo_crs['libraries'][0]['name'] = 'main'
    new_foo = dir_renderer.get_or_render('new-foo', {
        'pkg.json': json.dumps(new_foo_crs),
        'src': {
            'foo.hpp': '#pragma once\n inline int get_value() { return 42; }\n'
        }
    })
    repo.repo.import_(new_foo)
    tmp_project.build(repos=[repo.path])


def test_solve_cand_missing_libs(solve_repo_1: QuickRepo) -> None:
    """
    Check that we reject if the only candidate does not have the libraries that we need.
    """
    with expect_solve_fail():
        solve_repo_1.pkg_solve('foo@1.3.1 using no-such-lib')


def test_solve_skip_for_req_libs(solve_repo_1: QuickRepo) -> None:
    with expect_solve_fail():
        solve_repo_1.pkg_solve('pkg-adds-library=1.2.3 using another')
    solve_repo_1.pkg_solve('pkg-adds-library^1.2.3 using another')


def test_solve_transitive_req_requires_lib_conflict(solve_repo_1: QuickRepo) -> None:
    solve_repo_1.pkg_solve('pkg-conflicting-libset^1.2.3 using another')


def test_solve_unsolvable_version_diamond(solve_repo_1: QuickRepo):
    with expect_solve_fail():
        solve_repo_1.pkg_solve('pkg-unsolvable-version-diamond@1.2.3')


def test_solve_unsolvable_lib_diamond(solve_repo_1: QuickRepo):
    with expect_solve_fail():
        solve_repo_1.pkg_solve('pkg-unsolvable-lib-diamond@1.2.3')


def test_solve_ignore_unsolvable_libs(solve_repo_1: QuickRepo) -> None:
    # No error: We aren't depending on the 'nonesuch' library
    with expect_solve_fail():
        solve_repo_1.pkg_solve('pkg-with-unsolvable-library@1.2.3')
    solve_repo_1.pkg_solve('pkg-with-unsolvable-library@1.2.3 using goodlib')
    with expect_solve_fail():
        solve_repo_1.pkg_solve('pkg-with-unsolvable-library@1.2.3 using badlib')


def test_solve_skip_unsolvable_version(solve_repo_1: QuickRepo) -> None:
    with expect_solve_fail():
        solve_repo_1.pkg_solve('pkg-with-unsolvable-version=1.2.3')
    # Okay: Just pull bar@1.2.4
    solve_repo_1.pkg_solve('pkg-with-unsolvable-version@1.2.3')


def test_solve_fail_with_transitive_no_such_lib(solve_repo_1: QuickRepo) -> None:
    # Error: No 'bar' has 'no-such-lib'
    with expect_solve_fail():
        solve_repo_1.pkg_solve('pkg-transitive-no-such-lib@1.2.3')


def test_solve_conflicting_libset_2(solve_repo_1: QuickRepo) -> None:
    with expect_solve_fail():
        solve_repo_1.pkg_solve('pkg-conflicting-libset-2@1.2.3 using main')
    with expect_solve_fail():
        solve_repo_1.pkg_solve('pkg-conflicting-libset-2@1.2.4 using main')
    solve_repo_1.pkg_solve('pkg-conflicting-libset-2@1.2.3 using nope')
