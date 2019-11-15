#!/usr/bin/env python3

import argparse
from contextlib import contextmanager
import os
from pathlib import Path
from typing import Sequence
import subprocess
import sys
import shutil
import tempfile

from dds_ci import paths
from self_build import self_build
from self_deps_get import self_deps_get
from self_deps_build import self_deps_build

ROOT = Path(__file__).parent.parent.absolute()
BUILD_DIR = ROOT / '_build'


@contextmanager
def _generate_toolchain(cxx: str):
    with tempfile.NamedTemporaryFile(
            suffix='-dds-toolchain.dds', mode='wb', delete=False) as f:
        comp_id = 'GNU'
        flags = ''
        link_flags = ''
        if cxx in ('cl', 'cl.exe'):
            comp_id = 'MSVC'
            flags += '/experimental:preprocessor '
            link_flags += 'rpcrt4.lib '
        else:
            flags += '-fconcepts'
        flags += ' -DSPDLOG_COMPILED_LIB'
        content = f'''
            Compiler-ID: {comp_id}
            C++-Compiler: {cxx}
            C++-Version: C++17
            Debug: True
            Flags: {flags}
            Link-Flags: {link_flags}
        '''
        print('Using generated toolchain file: ' + content)
        f.write(content.encode('utf-8'))
        f.close()
        yield Path(f.name)
        os.unlink(f.name)


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--cxx', help='Path/name of the C++ compiler to use.', required=True)
    args = parser.parse_args(argv)

    dds_bootstrap_env_key = 'DDS_BOOTSTRAP_PREV_EXE'
    if dds_bootstrap_env_key not in os.environ:
        raise RuntimeError('A previous-phase bootstrapped executable '
                           'must be available via $DDS_BOOTSTRAP_PREV_EXE')

    dds_exe = Path(os.environ[dds_bootstrap_env_key])

    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)

    print(f'Using previously built DDS executable: {dds_exe}')
    self_deps_get(dds_exe, paths.SELF_TEST_REPO_DIR)

    with _generate_toolchain(args.cxx) as tc_fpath:
        self_deps_build(dds_exe, tc_fpath, paths.SELF_TEST_REPO_DIR,
                        ROOT / 'remote.dds')
        self_build(dds_exe, toolchain=tc_fpath, dds_flags=['--apps'])

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
