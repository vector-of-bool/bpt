import json

from tests import dds, DDS
from tests.fileutil import ensure_dir


def test_get(dds: DDS):
    dds.scope.enter_context(ensure_dir(dds.build_dir))
    cat_path = dds.build_dir / 'catalog.db'
    dds.catalog_create(cat_path)

    json_path = dds.build_dir / 'catalog.json'
    import_data = {
        'version': 1,
        'packages': {
            'neo-sqlite3': {
                '0.2.2': {
                    'depends': {},
                    'git': {
                        'url': 'https://github.com/vector-of-bool/neo-sqlite3.git',
                        'ref': '0.2.2',
                    },
                },
            },
        },
    }
    dds.scope.enter_context(
        dds.set_contents(json_path,
                         json.dumps(import_data).encode()))

    dds.catalog_import(cat_path, json_path)

    dds.catalog_get(cat_path, 'neo-sqlite3@0.2.2')
