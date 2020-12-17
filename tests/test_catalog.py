from dds_ci.testing import Project, RepoFixture


def test_pkg_get(http_repo: RepoFixture, tmp_project: Project) -> None:
    http_repo.import_json_data({
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
    })
    tmp_project.dds.repo_add(http_repo.url)
    tmp_project.dds.pkg_get('neo-sqlite3@0.3.0')
    assert tmp_project.root.joinpath('neo-sqlite3@0.3.0').is_dir()
    assert tmp_project.root.joinpath('neo-sqlite3@0.3.0/package.jsonc').is_file()
