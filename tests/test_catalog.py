from pathlib import Path

from dds_ci.testing import Project, RepoFixture
from dds_ci.dds import DDSWrapper


def test_catalog_create(dds_2: DDSWrapper, tmp_path: Path) -> None:
    cat_db = tmp_path / 'catalog.db'
    assert not cat_db.is_file()
    dds_2.run(['catalog', 'create', '--catalog', cat_db])
    assert cat_db.is_file()


def test_catalog_get_git(http_repo: RepoFixture, tmp_project: Project) -> None:
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
    tmp_project.dds.catalog_get('neo-sqlite3@0.3.0')
    assert tmp_project.root.joinpath('neo-sqlite3@0.3.0').is_dir()
    assert tmp_project.root.joinpath('neo-sqlite3@0.3.0/package.jsonc').is_file()
