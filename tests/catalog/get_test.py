import json

from tests import dds, DDS
from tests.fileutil import ensure_dir

import pytest


def test_get(dds: DDS):
    dds.scope.enter_context(ensure_dir(dds.build_dir))
    dds.catalog_create()

    json_path = dds.build_dir / 'catalog.json'
    import_data = {
        'version': 2,
        'packages': {
            'neo-sqlite3': {
                '0.3.0': {
                    'url':
                    'git+https://github.com/vector-of-bool/neo-sqlite3.git#0.3.0',
                },
            },
        },
    }
    dds.scope.enter_context(
        dds.set_contents(json_path,
                         json.dumps(import_data).encode()))

    dds.catalog_import(json_path)

    dds.catalog_get('neo-sqlite3@0.3.0')
    assert (dds.source_root / 'neo-sqlite3@0.3.0').is_dir()
    assert (dds.source_root / 'neo-sqlite3@0.3.0/package.jsonc').is_file()
