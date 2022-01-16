from typing import Any
from operator import itemgetter
from pathlib import Path
import urllib.request
import json
from string import Template


def http_get(url: str) -> bytes:
    return urllib.request.urlopen(url).read()


print('Downloading SPDX licenses')
licenses = json.loads(http_get('https://github.com/spdx/license-list-data/raw/master/json/licenses.json'))
exceptions = json.loads(http_get('https://github.com/spdx/license-list-data/raw/master/json/exceptions.json'))

ITEM_TEMPLATE = Template(r'''SPDX_LICENSE($id, $name)''')


def quote(s: str) -> str:
    return f'R"_({s})_"'


def render_license(dat: Any) -> str:
    id_ = dat['licenseId']
    name = dat['name']
    return f'SPDX_LICENSE({quote(id_)}, {quote(name)})'


def render_exception(dat: Any) -> str:
    id_ = dat['licenseExceptionId']
    name = dat['name']
    return f'SPDX_EXCEPTION({quote(id_)}, {quote(name)})'


lic_lines = (render_license(l) for l in sorted(licenses['licenses'], key=itemgetter('licenseId')))
exc_lines = (render_exception(e) for e in sorted(exceptions['exceptions'], key=itemgetter('licenseExceptionId')))

proj_dir = Path(__file__).resolve().parent.parent / 'src/dds/project'
print('Writing license content')
proj_dir.joinpath('spdx.inl').write_text('\n'.join(lic_lines))
proj_dir.joinpath('spdx-exc.inl').write_text('\n'.join(exc_lines))
