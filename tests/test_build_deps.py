import json

import pytest

from dds_ci.testing import RepoServer, Project, ProjectOpener
from dds_ci import proc, toolchain

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
def test_repo(http_repo: RepoServer) -> RepoServer:
    http_repo.import_json_data(SIMPLE_CATALOG)
    return http_repo


@pytest.fixture()
def test_project(tmp_project: Project, test_repo: RepoServer) -> Project:
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


def test_cmake_simple(project_opener: ProjectOpener) -> None:
    proj = project_opener.open('projects/simple')
    proj.dds.pkg_import(proj.root)

    cm_proj_dir = project_opener.test_dir / 'projects/simple-cmake'
    proj.build_root.mkdir(exist_ok=True, parents=True)
    proj.dds.run(
        [
            'build-deps',
            proj.dds.repo_dir_arg,
            'foo@1.2.3',
            ('-t', ':gcc' if 'gcc' in toolchain.get_default_toolchain().name else ':msvc'),
            f'--cmake=libraries.cmake',
        ],
        cwd=proj.build_root,
    )

    try:
        proc.check_run(['cmake', '-S', cm_proj_dir, '-B', proj.build_root])
    except FileNotFoundError:
        assert False, 'Running the integration tests requires a CMake executable'
    proc.check_run(['cmake', '--build', proj.build_root])
