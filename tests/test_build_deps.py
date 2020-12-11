import json

import pytest

from dds_ci.testing import RepoFixture, Project

SIMPLE_CATALOG = {
    "packages": {
        "neo-fun": {
            "0.3.0": {
                "remote": {
                    "git": {
                        "url": "https://github.com/vector-of-bool/neo-fun.git",
                        "ref": "0.3.0"
                    }
                }
            }
        }
    }
}


@pytest.fixture()
def test_repo(http_repo: RepoFixture) -> RepoFixture:
    http_repo.import_json_data(SIMPLE_CATALOG)
    return http_repo


@pytest.fixture()
def test_project(tmp_project: Project, test_repo: RepoFixture) -> Project:
    tmp_project.dds.repo_add(test_repo.url)
    return tmp_project


def test_from_file(test_project: Project) -> None:
    """build-deps using a file listing deps"""
    test_project.write('deps.json5', json.dumps({'depends': ['neo-fun+0.3.0']}))
    test_project.dds.build_deps(['-d', 'deps.json5'])
    assert test_project.root.joinpath('_deps/neo-fun@0.3.0').is_dir()
    assert test_project.root.joinpath('_deps/_libman/neo-fun.lmp').is_file()
    assert test_project.root.joinpath('_deps/_libman/neo/fun.lml').is_file()
    assert test_project.root.joinpath('INDEX.lmi').is_file()


def test_from_cmd(test_project: Project) -> None:
    """build-deps using a command-line listing"""
    test_project.dds.build_deps(['neo-fun=0.3.0'])
    assert test_project.root.joinpath('_deps/neo-fun@0.3.0').is_dir()
    assert test_project.root.joinpath('_deps/_libman/neo-fun.lmp').is_file()
    assert test_project.root.joinpath('_deps/_libman/neo/fun.lml').is_file()
    assert test_project.root.joinpath('INDEX.lmi').is_file()


def test_multiple_deps(test_project: Project) -> None:
    """build-deps with multiple deps resolves to a single version"""
    test_project.dds.build_deps(['neo-fun^0.2.0', 'neo-fun~0.3.0'])
    assert test_project.root.joinpath('_deps/neo-fun@0.3.0').is_dir()
    assert test_project.root.joinpath('_deps/_libman/neo-fun.lmp').is_file()
    assert test_project.root.joinpath('_deps/_libman/neo/fun.lml').is_file()
    assert test_project.root.joinpath('INDEX.lmi').is_file()
