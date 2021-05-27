import pytest

from dds_ci import proc, paths
from dds_ci.testing import ProjectOpener
from dds_ci.testing.http import RepoServer

CATCH2_CATALOG = {
    "packages": {
        "catch2": {
            "2.12.4": {
                "remote": {
                    "git": {
                        "url": "https://github.com/catchorg/Catch2.git",
                        "ref": "v2.12.4"
                    },
                    'auto-lib': 'catch2/catch2',
                    'transform': [{
                        'copy': {
                            'from': 'include/',
                            'to': 'src/',
                        },
                        'move': {
                            'from': 'include/',
                            'to': 'include/catch2/',
                        },
                        'remove': {
                            'path': 'include/',
                            'only-matching': ['**/*.cpp']
                        },
                        'write': {
                            'path': 'include/catch2/catch_with_main.hpp',
                            'content': '''
                                #pragma once

                                #define CATCH_CONFIG_MAIN
                                #include "./catch.hpp"

                                namespace Catch {

                                CATCH_REGISTER_REPORTER("console", ConsoleReporter)

                                }
                            '''
                        }
                    }]
                }
            }
        }
    }
}


@pytest.fixture(autouse=True)
def _init_repo(module_http_repo: RepoServer) -> RepoServer:
    module_http_repo.import_json_data(CATCH2_CATALOG)
    return module_http_repo



def test_main(module_http_repo: RepoServer, project_opener: ProjectOpener) -> None:
    proj = project_opener.open('main')
    proj.dds.repo_add(module_http_repo.url)
    proj.build()
    test_exe = proj.build_root.joinpath('test/testlib/calc' + paths.EXE_SUFFIX)
    assert test_exe.is_file()
    assert proc.run([test_exe]).returncode == 0


def test_custom(module_http_repo: RepoServer, project_opener: ProjectOpener) -> None:
    proj = project_opener.open('custom-runner')
    proj.dds.repo_add(module_http_repo.url)
    proj.build()
    test_exe = proj.build_root.joinpath('test/testlib/calc' + paths.EXE_SUFFIX)
    assert test_exe.is_file()
    assert proc.run([test_exe]).returncode == 0
