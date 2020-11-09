import json
from contextlib import contextmanager

from tests import dds, DDS
from tests.fileutil import ensure_dir

import pytest


def load_catalog(dds: DDS, data):
    dds.scope.enter_context(ensure_dir(dds.build_dir))
    dds.catalog_create()

    json_path = dds.build_dir / 'catalog.json'
    dds.scope.enter_context(
        dds.set_contents(json_path,
                         json.dumps(data).encode()))
    dds.catalog_import(json_path)


def test_get(dds: DDS):
    load_catalog(
        dds, {
            'version': 2,
            'packages': {
                'neo-sqlite3': {
                    '0.3.0': {
                        'url':
                        'git+https://github.com/vector-of-bool/neo-sqlite3.git#0.3.0',
                    },
                },
            },
        })

    dds.catalog_get('neo-sqlite3@0.3.0')
    assert (dds.scratch_dir / 'neo-sqlite3@0.3.0').is_dir()
    assert (dds.scratch_dir / 'neo-sqlite3@0.3.0/package.jsonc').is_file()


def test_get_http(dds: DDS):
    load_catalog(
        dds, {
            'version': 2,
            'packages': {
                'cmcstl2': {
                    '2020.2.24': {
                        'url':
                        'https://github.com/CaseyCarter/cmcstl2/archive/684a96d527e4dc733897255c0177b784dc280980.tar.gz?dds_lm=cmc/stl2;',
                    },
                },
            },
        })
    dds.catalog_get('cmcstl2@2020.2.24')
    assert dds.scratch_dir.joinpath('cmcstl2@2020.2.24/include').is_dir()
