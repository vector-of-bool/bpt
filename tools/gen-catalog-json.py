import json
from typing import NamedTuple, Tuple, List, Sequence, Union, Optional, Mapping
import sys


class Git(NamedTuple):
    url: str
    ref: str
    auto_lib: Optional[str] = None

    def to_dict(self) -> dict:
        return {
            'url': self.url,
            'ref': self.ref,
            'auto-lib': self.auto_lib,
        }


RemoteInfo = Union[Git]


class Version(NamedTuple):
    version: str
    remote: RemoteInfo
    depends: Mapping[str, str] = {}

    def to_dict(self) -> dict:
        ret = {}
        ret['depends'] = {}
        if isinstance(self.remote, Git):
            ret['git'] = self.remote.to_dict()
        return ret


class Package(NamedTuple):
    name: str
    versions: List[Version]


def many_versions(name: str,
                  versions: Sequence[str],
                  *,
                  tag_fmt: str = '{}',
                  git_url: str,
                  auto_lib: str = None) -> Package:
    return Package(name, [
        Version(
            ver,
            remote=Git(
                url=git_url, ref=tag_fmt.format(ver), auto_lib=auto_lib))
        for ver in versions
    ])


packages = [
    many_versions(
        'range-v3',
        (
            '0.5.0',
            '0.9.0',
            '0.9.1',
            '0.10.0',
        ),
        git_url='https://github.com/ericniebler/range-v3.git',
        auto_lib='Niebler/range-v3',
    ),
    many_versions(
        'nlohmann-json',
        (
            '3.0.0',
            '3.0.1',
            '3.1.0',
            '3.1.1',
            '3.1.2',
            '3.2.0',
            '3.3.0',
            '3.4.0',
            '3.5.0',
            '3.6.0',
            '3.6.1',
            '3.7.0',
            '3.7.1',
            '3.7.2',
            '3.7.3',
        ),
        git_url='https://github.com/vector-of-bool/json.git',
        tag_fmt='dds/{}',
    ),
    Package('ms-wil', [
        Version(
            '2019.11.10',
            remote=Git('https://github.com/vector-of-bool/wil.git',
                       'dds/2019.11.10'))
    ]),
    many_versions(
        'neo-sqlite3',
        (
            '0.1.0',
            '0.2.0',
            '0.2.1',
        ),
        git_url='https://github.com/vector-of-bool/neo-sqlite3.git',
    ),
    Package('neo-fun', [
        Version(
            '0.1.0',
            remote=Git('https://github.com/vector-of-bool/neo-fun.git',
                       '0.1.0'))
    ]),
    Package('semver', [
        Version(
            '0.2.1',
            remote=Git('https://github.com/vector-of-bool/semver.git',
                       '0.2.1'))
    ]),
    many_versions(
        'pubgrub',
        (
            '0.1.2',
            '0.2.0',
        ),
        git_url='https://github.com/vector-of-bool/pubgrub.git',
    ),
    many_versions(
        'spdlog',
        (
            '0.9.0',
            '0.10.0',
            '0.11.0',
            '0.12.0',
            '0.13.0',
            '0.14.0',
            '0.16.0',
            '0.16.1',
            '0.16.2',
            '0.17.0',
            '1.0.0',
            '1.1.0',
            '1.2.0',
            '1.2.1',
            '1.3.0',
            '1.3.1',
            '1.4.0',
            '1.4.1',
            '1.4.1',
        ),
        git_url='https://github.com/gabime/spdlog.git',
        tag_fmt='v{}',
        auto_lib='spdlog/spdlog',
    ),
    many_versions(
        'fmt',
        (
            '0.8.0',
            '0.9.0',
            '0.10.0',
            '0.12.0',
            '1.0.0',
            '1.1.0',
            '2.0.0',
            '2.0.1',
            '2.1.0',
            '2.1.1',
            '3.0.0',
            '3.0.1',
            '3.0.2',
            '4.0.0',
            '4.1.0',
            '5.0.0',
            '5.1.0',
            '5.2.0',
            '5.2.1',
            '5.3.0',
            '6.0.0',
            '6.1.0',
            '6.1.1',
            '6.1.2',
        ),
        git_url='https://github.com/fmtlib/fmt.git',
        auto_lib='fmt/fmt',
    ),
]

data = {
    'version': 1,
    'packages': {
        pkg.name: {ver.version: ver.to_dict()
                   for ver in pkg.versions}
        for pkg in packages
    }
}

print(json.dumps(data, indent=2, sort_keys=True))