import json

from tests import dds, DDS
from tests.fileutil import ensure_dir


def test_import_json(dds: DDS):
    dds.scope.enter_context(ensure_dir(dds.build_dir))
    dds.catalog_create()

    json_fpath = dds.build_dir / 'data.json'
    import_data = {
        'version': 2,
        'packages': {
            'foo': {
                '1.2.4': {
                    'url': 'git+http://example.com#master',
                    'depends': [],
                },
                '1.2.5': {
                    'url': 'git+http://example.com#master',
                },
            },
        },
    }
    dds.scope.enter_context(
        dds.set_contents(json_fpath,
                         json.dumps(import_data).encode()))
    dds.catalog_import(json_fpath)
