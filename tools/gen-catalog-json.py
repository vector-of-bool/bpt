import argparse
import json
import itertools
from typing import NamedTuple, Tuple, List, Sequence, Union, Optional, Mapping
from pathlib import Path
import sys
import textwrap


class CopyMoveTransform(NamedTuple):
    frm: str
    to: str
    strip_components: int = 0
    include: Sequence[str] = []
    exclude: Sequence[str] = []

    def to_dict(self):
        return {
            'from': self.frm,
            'to': self.to,
            'include': self.include,
            'exclude': self.exclude,
            'strip-components': self.strip_components,
        }


class OneEdit(NamedTuple):
    kind: str
    line: int
    content: Optional[str] = None

    def to_dict(self):
        d = {
            'kind': self.kind,
            'line': self.line,
        }
        if self.content:
            d['content'] = self.content
        return d


class EditTransform(NamedTuple):
    path: str
    edits: Sequence[OneEdit] = []

    def to_dict(self):
        return {
            'path': self.path,
            'edits': [e.to_dict() for e in self.edits],
        }


class WriteTransform(NamedTuple):
    path: str
    content: str

    def to_dict(self):
        return {
            'path': self.path,
            'content': self.content,
        }


class RemoveTransform(NamedTuple):
    path: str
    only_matching: Sequence[str] = ()

    def to_dict(self):
        return {
            'path': self.path,
            'only-matching': self.only_matching,
        }


class FSTransform(NamedTuple):
    copy: Optional[CopyMoveTransform] = None
    move: Optional[CopyMoveTransform] = None
    remove: Optional[RemoveTransform] = None
    write: Optional[WriteTransform] = None
    edit: Optional[EditTransform] = None

    def to_dict(self):
        d = {}
        if self.copy:
            d['copy'] = self.copy.to_dict()
        if self.move:
            d['move'] = self.move.to_dict()
        if self.remove:
            d['remove'] = self.remove.to_dict()
        if self.write:
            d['write'] = self.write.to_dict()
        if self.edit:
            d['edit'] = self.edit.to_dict()
        return d


class Git(NamedTuple):
    url: str
    ref: str
    auto_lib: Optional[str] = None
    transforms: Sequence[FSTransform] = []

    def to_dict(self) -> dict:
        d = {
            'url': self.url,
            'ref': self.ref,
            'transform': [f.to_dict() for f in self.transforms],
        }
        if self.auto_lib:
            d['auto-lib'] = self.auto_lib
        return d


RemoteInfo = Union[Git]


class Version(NamedTuple):
    version: str
    remote: RemoteInfo
    depends: Mapping[str, str] = {}
    description: str = '(No description provided)'

    def to_dict(self, new=False) -> dict:
        ret: dict = {
            'description': self.description,
        }
        if new:
            ret['depends'] = [k + v for k, v in self.depends.items()]
        else:
            ret['depends'] = self.depends
        if isinstance(self.remote, Git):
            ret['git'] = self.remote.to_dict()
        return ret


class VersionSet(NamedTuple):
    version: str
    depends: Sequence[Tuple[str, str]]


class Package(NamedTuple):
    name: str
    versions: List[Version]


def simple_packages(name: str,
                    description: str,
                    git_url: str,
                    versions: Sequence[VersionSet],
                    auto_lib: Optional[str] = None,
                    *,
                    tag_fmt: str = '{}') -> Package:
    return Package(name, [
        Version(
            ver.version,
            description=description,
            remote=Git(
                git_url, tag_fmt.format(ver.version), auto_lib=auto_lib),
            depends={dep_name: dep_rng
                     for dep_name, dep_rng in ver.depends}) for ver in versions
    ])


def many_versions(name: str,
                  versions: Sequence[str],
                  *,
                  tag_fmt: str = '{}',
                  git_url: str,
                  auto_lib: str = None,
                  transforms: Sequence[FSTransform] = (),
                  description='(No description was provided)') -> Package:
    return Package(name, [
        Version(
            ver,
            description='\n'.join(textwrap.wrap(description)),
            remote=Git(
                url=git_url,
                ref=tag_fmt.format(ver),
                auto_lib=auto_lib,
                transforms=transforms)) for ver in versions
    ])


PACKAGES = [
    many_versions(
        'magic_enum',
        (
            '0.5.0',
            '0.6.0',
            '0.6.1',
            '0.6.2',
            '0.6.3',
            '0.6.4',
            '0.6.5',
            '0.6.6',
        ),
        description='Static reflection for enums',
        tag_fmt='v{}',
        git_url='https://github.com/Neargye/magic_enum.git',
        auto_lib='neargye/magic_enum',
    ),
    many_versions(
        'nameof',
        [
            '0.8.3',
            '0.9.0',
            '0.9.1',
            '0.9.2',
            '0.9.3',
            '0.9.4',
        ],
        description='Nameof operator for modern C++',
        tag_fmt='v{}',
        git_url='https://github.com/Neargye/nameof.git',
        auto_lib='neargye/nameof',
    ),
    many_versions(
        'range-v3',
        (
            '0.5.0',
            '0.9.0',
            '0.9.1',
            '0.10.0',
        ),
        git_url='https://github.com/ericniebler/range-v3.git',
        auto_lib='range-v3/range-v3',
        description=
        'Range library for C++14/17/20, basis for C++20\'s std::ranges',
    ),
    many_versions(
        'nlohmann-json',
        (
            # '3.0.0',
            # '3.0.1',
            # '3.1.0',
            # '3.1.1',
            # '3.1.2',
            # '3.2.0',
            # '3.3.0',
            # '3.4.0',
            # '3.5.0',
            # '3.6.0',
            # '3.6.1',
            # '3.7.0',
            '3.7.1',  # Only this version has the dds forked branch
            # '3.7.2',
            # '3.7.3',
        ),
        git_url='https://github.com/vector-of-bool/json.git',
        tag_fmt='dds/{}',
        description='JSON for Modern C++',
    ),
    Package('ms-wil', [
        Version(
            '2020.03.16',
            description='The Windows Implementation Library',
            remote=Git('https://github.com/vector-of-bool/wil.git',
                       'dds/2020.03.16'))
    ]),
    many_versions(
        'neo-sqlite3',
        (
            '0.1.0',
            '0.2.0',
            '0.2.1',
            '0.2.2',
            '0.2.3',
            '0.3.0',
        ),
        description='A modern and low-level C++ SQLite API',
        git_url='https://github.com/vector-of-bool/neo-sqlite3.git',
    ),
    many_versions(
        'neo-fun',
        (
            '0.1.0',
            '0.1.1',
            '0.2.0',
            '0.2.1',
            '0.3.0',
            '0.3.1',
            '0.3.2',
        ),
        description='Some library fundamentals that you might find useful',
        git_url='https://github.com/vector-of-bool/neo-fun.git',
    ),
    many_versions(
        'neo-concepts',
        (
            '0.1.0',
            '0.2.0',
            '0.2.1',
            '0.2.2',
            '0.3.0',
            '0.3.1',
            '0.3.2',
        ),
        description=
        'Minimal C++ concepts library. Contains many definitions from C++20.',
        git_url='https://github.com/vector-of-bool/neo-concepts.git',
    ),
    Package('semver', [
        Version(
            '0.2.1',
            description=
            'A C++ library that implements Semantic Versioning parsing, emitting, '
            'types, ordering, and operations. See https://semver.org/',
            remote=Git('https://github.com/vector-of-bool/semver.git',
                       '0.2.1')),
        Version(
            '0.2.2',
            description=
            'A C++ library that implements Semantic Versioning parsing, emitting, '
            'types, ordering, and operations. See https://semver.org/',
            remote=Git('https://github.com/vector-of-bool/semver.git',
                       '0.2.2')),
    ]),
    many_versions(
        'pubgrub',
        (
            '0.1.2',
            '0.2.0',
            '0.2.1',
        ),
        description=
        'A C++ implementation of the Pubgrub version solving algorithm',
        git_url='https://github.com/vector-of-bool/pubgrub.git',
    ),
    many_versions(
        'vob-json5',
        ('0.1.5', ),
        description='A C++ implementation of a JSON5 parser',
        git_url='https://github.com/vector-of-bool/json5.git',
    ),
    simple_packages(
        'vob-semester',
        description='A C++ library to process recursive dynamic data',
        git_url='https://github.com/vector-of-bool/semester.git',
        versions=[
            VersionSet('0.1.0', [
                ('neo-fun', '^0.1.0'),
                ('neo-concepts', '^0.2.1'),
            ]),
            VersionSet('0.1.1', [
                ('neo-fun', '^0.1.1'),
                ('neo-concepts', '^0.2.2'),
            ]),
            VersionSet('0.2.0', [
                ('neo-fun', '^0.3.2'),
                ('neo-concepts', '^0.3.2'),
            ]),
            VersionSet('0.2.1', [
                ('neo-fun', '^0.3.2'),
                ('neo-concepts', '^0.3.2'),
            ]),
        ],
    ),
    many_versions(
        'ctre',
        (
            '2.8.1',
            '2.8.2',
            '2.8.3',
            '2.8.4',
        ),
        git_url=
        'https://github.com/hanickadot/compile-time-regular-expressions.git',
        tag_fmt='v{}',
        auto_lib='hanickadot/ctre',
        description=
        'A compile-time PCRE (almost) compatible regular expression matcher',
    ),
    many_versions(
        'spdlog',
        (
            '1.0.0',
            '1.1.0',
            '1.2.0',
            '1.2.1',
            '1.3.0',
            '1.3.1',
            '1.4.0',
            '1.4.1',
            '1.4.2',
        ),
        git_url='https://github.com/gabime/spdlog.git',
        tag_fmt='v{}',
        auto_lib='spdlog/spdlog',
        description='Fast C++ logging library',
    ),
    many_versions(
        'fmt',
        (
            '6.0.0',
            '6.1.0',
            '6.1.1',
            '6.1.2',
            '6.2.0',
            '6.2.1',
            '7.0.0',
            '7.0.1',
        ),
        git_url='https://github.com/fmtlib/fmt.git',
        auto_lib='fmt/fmt',
        description='A modern formatting library : https://fmt.dev/',
    ),
    Package('catch2', [
        Version(
            '2.12.4',
            description='A modern C++ unit testing library',
            remote=Git(
                'https://github.com/catchorg/Catch2.git',
                'v2.12.4',
                auto_lib='catch2/catch2',
                transforms=[
                    FSTransform(
                        move=CopyMoveTransform(
                            frm='include', to='include/catch2')),
                    FSTransform(
                        copy=CopyMoveTransform(frm='include', to='src'),
                        write=WriteTransform(
                            path='include/catch2/catch_with_main.hpp',
                            content='''
                    #pragma once

                    #define CATCH_CONFIG_MAIN
                    #include "./catch.hpp"

                    namespace Catch {

                    CATCH_REGISTER_REPORTER("console", ConsoleReporter)

                    }
                    ''')),
                ]))
    ]),
    Package('asio', [
        Version(
            ver,
            description='Asio asynchronous I/O C++ library',
            remote=Git(
                'https://github.com/chriskohlhoff/asio.git',
                f'asio-{ver.replace(".", "-")}',
                auto_lib='asio/asio',
                transforms=[
                    FSTransform(
                        move=CopyMoveTransform(
                            frm='asio/src',
                            to='src/',
                        ),
                        remove=RemoveTransform(
                            path='src/',
                            only_matching=[
                                'doc/**',
                                'examples/**',
                                'tests/**',
                                'tools/**',
                            ],
                        ),
                    ),
                    FSTransform(
                        move=CopyMoveTransform(
                            frm='asio/include/',
                            to='include/',
                        ),
                        edit=EditTransform(
                            path='include/asio/detail/config.hpp',
                            edits=[
                                OneEdit(
                                    line=13,
                                    kind='insert',
                                    content='#define ASIO_STANDALONE 1'),
                                OneEdit(
                                    line=14,
                                    kind='insert',
                                    content=
                                    '#define ASIO_SEPARATE_COMPILATION 1')
                            ]),
                    ),
                ]),
        ) for ver in [
            '1.12.0',
            '1.12.1',
            '1.12.2',
            '1.13.0',
            '1.14.0',
            '1.14.1',
            '1.16.0',
            '1.16.1',
        ]
    ]),
    Package(
        'abseil',
        [
            Version(
                ver,
                description='Abseil Common Libraries',
                remote=Git(
                    'https://github.com/abseil/abseil-cpp.git',
                    tag,
                    auto_lib='abseil/abseil',
                    transforms=[
                        FSTransform(
                            move=CopyMoveTransform(
                                frm='absl',
                                to='src/absl/',
                            ),
                            remove=RemoveTransform(
                                path='src/',
                                only_matching=[
                                    '**/*_test.c*',
                                    '**/*_testing.c*',
                                    '**/*_benchmark.c*',
                                    '**/benchmarks.c*',
                                    '**/*_test_common.c*',
                                    '**/mocking_*.c*',
                                    # Misc files that should be removed:
                                    '**/test_util.cc',
                                    '**/mutex_nonprod.cc',
                                    '**/named_generator.cc',
                                    '**/print_hash_of.cc',
                                    '**/*_gentables.cc',
                                ]),
                        )
                    ]),
            ) for ver, tag in [
                ('2018.6.0', '20180600'),
                ('2019.8.8', '20190808'),
                ('2020.2.25', '20200225.2'),
            ]
        ]),
    Package(
        'zlib',
        [
            Version(
                ver,
                description=
                'A massively spiffy yet delicately unobtrusive compression library',
                remote=Git(
                    'https://github.com/madler/zlib.git',
                    tag or f'v{ver}',
                    auto_lib='zlib/zlib',
                    transforms=[
                        FSTransform(
                            move=CopyMoveTransform(
                                frm='.',
                                to='src/',
                                include=[
                                    '*.c',
                                    '*.h',
                                ],
                            )),
                        FSTransform(
                            move=CopyMoveTransform(
                                frm='src/',
                                to='include/',
                                include=['zlib.h', 'zconf.h'],
                            )),
                    ]),
            ) for ver, tag in [
                ('1.2.11', None),
                ('1.2.10', None),
                ('1.2.9', None),
                ('1.2.8', None),
                ('1.2.7', 'v1.2.7.3'),
                ('1.2.6', 'v1.2.6.1'),
                ('1.2.5', 'v1.2.5.3'),
                ('1.2.4', 'v1.2.4.5'),
                ('1.2.3', 'v1.2.3.8'),
                ('1.2.2', 'v1.2.2.4'),
                ('1.2.1', 'v1.2.1.2'),
                ('1.2.0', 'v1.2.0.8'),
                ('1.1.4', None),
                ('1.1.3', None),
                ('1.1.2', None),
                ('1.1.1', None),
                ('1.1.0', None),
                ('1.0.9', None),
                ('1.0.8', None),
                ('1.0.7', None),
                # ('1.0.6', None),  # Does not exist
                ('1.0.5', None),
                ('1.0.4', None),
                # ('1.0.3', None),  # Does not exist
                ('1.0.2', None),
                ('1.0.1', None),
            ]
        ]),
    Package('sol2', [
        Version(
            ver,
            description=
            'A C++ <-> Lua API wrapper with advanced features and top notch performance',
            depends={'lua': '+0.0.0'},
            remote=Git(
                'https://github.com/ThePhD/sol2.git',
                f'v{ver}',
                transforms=[
                    FSTransform(
                        write=WriteTransform(
                            path='package.json',
                            content=json.dumps(
                                {
                                    'name': 'sol2',
                                    'namespace': 'sol2',
                                    'version': ver,
                                    'depends': [f'lua+0.0.0'],
                                },
                                indent=2,
                            )),
                        move=(None
                              if ver.startswith('3.') else CopyMoveTransform(
                                  frm='sol',
                                  to='src/sol',
                              )),
                    ),
                    FSTransform(
                        write=WriteTransform(
                            path='library.json',
                            content=json.dumps(
                                {
                                    'name': 'sol2',
                                    'uses': ['lua/lua'],
                                },
                                indent=2,
                            ))),
                ]),
        ) for ver in [
            '3.2.1',
            '3.2.0',
            '3.0.3',
            '3.0.2',
            '2.20.6',
            '2.20.5',
            '2.20.4',
            '2.20.3',
            '2.20.2',
            '2.20.1',
            '2.20.0',
        ]
    ]),
    Package('lua', [
        Version(
            ver,
            description=
            'Lua is a powerful and fast programming language that is easy to learn and use and to embed into your application.',
            remote=Git(
                'https://github.com/lua/lua.git',
                f'v{ver}',
                auto_lib='lua/lua',
                transforms=[
                    FSTransform(
                        move=CopyMoveTransform(
                            frm='.',
                            to='src/',
                            include=['*.c', '*.h'],
                        ))
                ]),
        ) for ver in [
            '5.4.0',
            '5.3.5',
            '5.3.4',
            '5.3.3',
            '5.3.2',
            '5.3.1',
            '5.3.0',
            '5.2.3',
            '5.2.2',
            '5.2.1',
            '5.2.0',
            '5.1.1',
        ]
    ]),
    Package('pegtl', [
        Version(
            ver,
            description='Parsing Expression Grammar Template Library',
            remote=Git(
                'https://github.com/taocpp/PEGTL.git',
                ver,
                auto_lib='tao/pegtl',
                transforms=[FSTransform(remove=RemoveTransform(path='src/'))],
            )) for ver in [
                '2.8.3',
                '2.8.2',
                '2.8.1',
                '2.8.0',
                '2.7.1',
                '2.7.0',
                '2.6.1',
                '2.6.0',
            ]
    ]),
    many_versions(
        'boost.pfr', ['1.0.0', '1.0.1'],
        auto_lib='boost/pfr',
        git_url='https://github.com/apolukhin/magic_get.git'),
    many_versions(
        'boost.leaf',
        [
            '0.1.0',
            '0.2.0',
            '0.2.1',
            '0.2.2',
            '0.2.3',
            '0.2.4',
            '0.2.5',
            '0.3.0',
        ],
        auto_lib='boost/leaf',
        git_url='https://github.com/zajo/leaf.git',
    ),
    many_versions(
        'boost.mp11',
        ['1.70.0', '1.71.0', '1.72.0', '1.73.0'],
        tag_fmt='boost-{}',
        git_url='https://github.com/boostorg/mp11.git',
        auto_lib='boost/mp11',
    ),
    many_versions(
        'libsodium', [
            '1.0.10',
            '1.0.11',
            '1.0.12',
            '1.0.13',
            '1.0.14',
            '1.0.15',
            '1.0.16',
            '1.0.17',
            '1.0.18',
        ],
        git_url='https://github.com/jedisct1/libsodium.git',
        auto_lib='sodium/sodium',
        description='Sodium is a new, easy-to-use software library '
        'for encryption, decryption, signatures, password hashing and more.',
        transforms=[
            FSTransform(
                move=CopyMoveTransform(
                    frm='src/libsodium/include', to='include/'),
                edit=EditTransform(
                    path='include/sodium/export.h',
                    edits=[
                        OneEdit(
                            line=8,
                            kind='insert',
                            content='#define SODIUM_STATIC 1')
                    ])),
            FSTransform(
                copy=CopyMoveTransform(
                    frm='builds/msvc/version.h',
                    to='include/sodium/version.h',
                ),
                move=CopyMoveTransform(
                    frm='src/libsodium',
                    to='src/',
                ),
                remove=RemoveTransform(path='src/libsodium'),
            ),
            FSTransform(
                copy=CopyMoveTransform(
                    frm='include', to='src/', strip_components=1)),
        ]),
    many_versions(
        'tomlpp',
        [
            '1.0.0',
            '1.1.0',
            '1.2.0',
            '1.2.3',
            '1.2.4',
            '1.2.5',
            '1.3.0',
            # '1.3.2',  # Wrong tag name in upstream
            '1.3.3',
        ],
        tag_fmt='v{}',
        git_url='https://github.com/marzer/tomlplusplus.git',
        auto_lib='tomlpp/tomlpp',
        description=
        'Header-only TOML config file parser and serializer for modern C++'),
    Package('inja', [
        *(Version(
            ver,
            description='A Template Engine for Modern C++',
            remote=Git(
                'https://github.com/pantor/inja.git',
                f'v{ver}',
                auto_lib='inja/inja')) for ver in ('1.0.0', '2.0.0', '2.0.1')),
        *(Version(
            ver,
            description='A Template Engine for Modern C++',
            depends={'nlohmann-json': '+0.0.0'},
            remote=Git(
                'https://github.com/pantor/inja.git',
                f'v{ver}',
                transforms=[
                    FSTransform(
                        write=WriteTransform(
                            path='package.json',
                            content=json.dumps({
                                'name':
                                'inja',
                                'namespace':
                                'inja',
                                'version':
                                ver,
                                'depends': [
                                    'nlohmann-json+0.0.0',
                                ]
                            }))),
                    FSTransform(
                        write=WriteTransform(
                            path='library.json',
                            content=json.dumps({
                                'name': 'inja',
                                'uses': ['nlohmann/json']
                            }))),
                ],
            )) for ver in ('2.1.0', '2.2.0')),
    ]),
    many_versions(
        'cereal',
        [
            '0.9.0',
            '0.9.1',
            '1.0.0',
            '1.1.0',
            '1.1.1',
            '1.1.2',
            '1.2.0',
            '1.2.1',
            '1.2.2',
            '1.3.0',
        ],
        auto_lib='cereal/cereal',
        git_url='https://github.com/USCiLab/cereal.git',
        tag_fmt='v{}',
        description='A C++11 library for serialization',
    ),
    many_versions(
        'pybind11',
        [
            '2.0.0',
            '2.0.1',
            '2.1.0',
            '2.1.1',
            '2.2.0',
            '2.2.1',
            '2.2.2',
            '2.2.3',
            '2.2.4',
            '2.3.0',
            '2.4.0',
            '2.4.1',
            '2.4.2',
            '2.4.3',
            '2.5.0',
        ],
        git_url='https://github.com/pybind/pybind11.git',
        description='Seamless operability between C++11 and Python',
        auto_lib='pybind/pybind11',
        tag_fmt='v{}',
    ),
    Package('pcg-cpp', [
        Version(
            '0.98.1',
            description='PCG Randum Number Generation, C++ Edition',
            remote=Git(
                url='https://github.com/imneme/pcg-cpp.git',
                ref='v0.98.1',
                auto_lib='pcg/pcg-cpp'))
    ]),
]

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    args = parser.parse_args()

    data_old = {
        'version': 1,
        'packages': {
            pkg.name:
            {ver.version: ver.to_dict(new=False)
             for ver in pkg.versions}
            for pkg in PACKAGES
        }
    }
    data_new = {
        'version': 1,
        'packages': {
            pkg.name:
            {ver.version: ver.to_dict(new=True)
             for ver in pkg.versions}
            for pkg in PACKAGES
        }
    }
    Path('catalog.old.json').write_text(
        json.dumps(data_old, indent=2, sort_keys=True))
    new_json_str = json.dumps(data_new, indent=2, sort_keys=True)
    Path('catalog.json').write_text(new_json_str)

    cpp_template = textwrap.dedent(r'''
        #include <dds/catalog/package_info.hpp>
        #include <dds/catalog/init_catalog.hpp>
        #include <dds/catalog/import.hpp>

        /**
         * The following array of integers is generated and contains the JSON
         * encoded initial catalog. MSVC can't handle string literals over
         * 64k large, so we have to resort to using a regular char array:
         */
        static constexpr const char INIT_PACKAGES_CONTENT[] = {
        @JSON@
        };

        static constexpr int INIT_PACKAGES_STR_LEN = @JSON_LEN@;

        const std::vector<dds::package_info>&
        dds::init_catalog_packages() noexcept {
            using std::nullopt;
            static auto pkgs = dds::parse_packages_json(
                std::string_view(INIT_PACKAGES_CONTENT, INIT_PACKAGES_STR_LEN));
            return pkgs;
        }
        ''')

    new_json_small = json.dumps(data_new, sort_keys=True)
    new_json_str_arr = ', '.join(str(ord(c)) for c in new_json_small)

    new_json_str_arr = '\n'.join(textwrap.wrap(new_json_str_arr, width=120))
    new_json_str_arr = textwrap.indent(new_json_str_arr, prefix=' ' * 4)

    cpp_content = cpp_template.replace('@JSON@', new_json_str_arr).replace(
        '@JSON_LEN@', str(len(new_json_small)))
    Path('src/dds/catalog/init_catalog.cpp').write_text(cpp_content)
