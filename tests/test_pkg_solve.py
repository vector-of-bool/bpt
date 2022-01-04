from typing import Callable, Dict, Sequence
import itertools
import pytest
import json

from typing_extensions import TypedDict, Protocol

from dds_ci.testing import CRSRepo, Project, error
from dds_ci.testing.fs import DirRenderer, TreeData
from dds_ci.testing.repo import CRSRepoFactory, make_simple_crs

_RepoPackageLibraryItem_Opt = TypedDict(
    '_RepoPackageLibraryItem_Opt',
    {
        'uses': Sequence[str],
        'depends': Sequence[str],
        'content': TreeData,
    },
    total=False,
)
_RepoPackageLibraryItem_Required = TypedDict('_RepoPackageLibraryItem', {})


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
                'uses': lib.get('uses', []),
                'depends': lib.get('depends', []),
            } for lib_name, lib in item['libs'].items()  #
        ]
    }
    return {
        'project.json': json.dumps(proj),
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


class QuickRepoFactory(Protocol):
    def __call__(self, name: str, spec: RepoSpec, validate: bool = True) -> CRSRepo:
        ...


@pytest.fixture(scope='session')
def make_quick_repo(dir_renderer: DirRenderer, crs_repo_factory: CRSRepoFactory) -> QuickRepoFactory:
    def _make(name: str, spec: RepoSpec, validate: bool = True) -> CRSRepo:
        data = _render_repospec(spec)
        content = dir_renderer.get_or_render(f'{name}-pkgs', data)
        repo = crs_repo_factory(name)
        for child in content.iterdir():
            repo.import_dir(child, validate=False)
        if validate:
            repo.validate()
        return repo

    return _make


@pytest.fixture(scope='session')
def solve_repo_1(make_quick_repo: QuickRepoFactory) -> CRSRepo:
    return make_quick_repo(
        'test1',
        validate=False,
        spec={
            'packages': {
                'foo': {
                    '1.3.1': {
                        'libs': {
                            'main': {
                                'uses': ['bar']
                            },
                            'bar': {},
                        },
                    }
                }
            },
        },
    )


def test_solve_1(solve_repo_1: CRSRepo, tmp_project: Project) -> None:
    tmp_project.dds.pkg_solve(repos=[solve_repo_1.path], pkgs=['foo@1.3.1 uses main'])


def test_solve_upgrade_meta_version(make_quick_repo: QuickRepoFactory, tmp_project: Project,
                                    dir_renderer: DirRenderer) -> None:
    '''
    Test that dds will pull a new copy of a package if its meta_version is updated,
    even if the version proper is not changed.
    '''
    repo = make_quick_repo(
        name='upgrade-meta-version',
        # yapf: disable
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
        # yapf: enable
    )
    tmp_project.project_json = {'name': 'test-proj', 'version': '1.2.3', 'depends': ['foo@1.2.3 uses main']}
    tmp_project.write('src/file.cpp', '#include <foo.hpp>\n')
    with error.expect_error_marker('compile-failed'):
        tmp_project.build(repos=[repo.path])

    # Create a new package that is good
    new_foo_crs = make_simple_crs('foo', '1.2.3', meta_version=2)
    new_foo_crs['libraries'][0]['name'] = 'main'
    new_foo = dir_renderer.get_or_render('new-foo', {
        'pkg.json': json.dumps(new_foo_crs),
        'src': {
            'foo.hpp': '#pragma once\n inline int get_value() { return 42; }\n'
        }
    })
    repo.import_dir(new_foo)
    tmp_project.build(repos=[repo.path])

