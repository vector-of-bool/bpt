from dds_ci.dds import DDSWrapper
from dds_ci.testing import Project, RepoFixture, PackageJSON
from dds_ci.testing.error import expect_error_marker

NEO_SQLITE_PKG_JSON = {
    'packages': {
        'neo-sqlite3': {
            '0.3.0': {
                'remote': {
                    'git': {
                        'url': 'https://github.com/vector-of-bool/neo-sqlite3.git',
                        'ref': '0.3.0',
                    }
                }
            }
        }
    }
}


def test_pkg_get(http_repo: RepoFixture, tmp_project: Project) -> None:
    http_repo.import_json_data(NEO_SQLITE_PKG_JSON)
    tmp_project.dds.repo_add(http_repo.url)
    tmp_project.dds.pkg_get('neo-sqlite3@0.3.0')
    assert tmp_project.root.joinpath('neo-sqlite3@0.3.0').is_dir()
    assert tmp_project.root.joinpath('neo-sqlite3@0.3.0/package.jsonc').is_file()


def test_pkg_repo(http_repo: RepoFixture, tmp_project: Project) -> None:
    dds = tmp_project.dds
    dds.repo_add(http_repo.url)
    dds.run(['pkg', 'repo', dds.catalog_path_arg, 'ls'])


def test_pkg_repo_rm(http_repo: RepoFixture, tmp_project: Project) -> None:
    http_repo.import_json_data(NEO_SQLITE_PKG_JSON)
    dds = tmp_project.dds
    dds.repo_add(http_repo.url)
    # Okay:
    tmp_project.dds.pkg_get('neo-sqlite3@0.3.0')
    # Remove the repo:
    dds.run(['pkg', dds.catalog_path_arg, 'repo', 'ls'])
    dds.repo_remove(http_repo.repo_name)
    # Cannot double-remove a repo:
    with expect_error_marker('repo-rm-no-such-repo'):
        dds.repo_remove(http_repo.repo_name)
    # Now, fails:
    with expect_error_marker('pkg-get-no-pkg-id-listing'):
        tmp_project.dds.pkg_get('neo-sqlite3@0.3.0')
