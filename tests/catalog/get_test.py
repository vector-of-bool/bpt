import json

from tests.fileutil import ensure_dir
from tests import dds, DDS
from tests.http import RepoFixture


def test_get(dds: DDS, http_repo: RepoFixture):
    http_repo.import_json_data({
        'version': 2,
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

    dds.repo_add(http_repo.url)
    dds.catalog_get('neo-sqlite3@0.3.0')
    assert (dds.scratch_dir / 'neo-sqlite3@0.3.0').is_dir()
    assert (dds.scratch_dir / 'neo-sqlite3@0.3.0/package.jsonc').is_file()


def test_get_http(dds: DDS, http_repo: RepoFixture):
    http_repo.import_json_data({
        'packages': {
            'cmcstl2': {
                '2020.2.24': {
                    'remote': {
                        'http': {
                            'url':
                            'https://github.com/CaseyCarter/cmcstl2/archive/684a96d527e4dc733897255c0177b784dc280980.tar.gz?dds_lm=cmc/stl2;',
                        },
                        'auto-lib': 'cmc/stl2',
                    }
                },
            },
        },
    })
    dds.scope.enter_context(ensure_dir(dds.source_root))
    dds.repo_add(http_repo.url)
    dds.catalog_get('cmcstl2@2020.2.24')
    assert dds.scratch_dir.joinpath('cmcstl2@2020.2.24/include').is_dir()
