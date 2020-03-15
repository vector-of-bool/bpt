#!/usr/bin/env python3

import argparse
import os
from pathlib import Path
from typing import Sequence
import sys
import shutil

from dds_ci import paths
from self_build import self_build
from self_deps_get import self_deps_get
from self_deps_build import self_deps_build

ROOT = Path(__file__).parent.parent.absolute()
BUILD_DIR = ROOT / '_build'


def main(argv: Sequence[str]) -> int:
    # Prior versions of this script took a --cxx argument, but we don't care anymore
    parser = argparse.ArgumentParser()
    parser.add_argument('--cxx', help=argparse.SUPPRESS)
    parser.parse_args(argv)

    dds_bootstrap_env_key = 'DDS_BOOTSTRAP_PREV_EXE'
    if dds_bootstrap_env_key not in os.environ:
        raise RuntimeError('A previous-phase bootstrapped executable '
                           'must be available via $DDS_BOOTSTRAP_PREV_EXE')

    dds_exe = Path(os.environ[dds_bootstrap_env_key])

    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)

    print(f'Using previously built DDS executable: {dds_exe}')
    self_deps_get(dds_exe, paths.SELF_TEST_REPO_DIR)

    if os.name == 'nt':
        tc_fpath = ROOT / 'tools/msvc.dds'
    elif sys.platform.startswith('freebsd'):
        tc_fpath = ROOT / 'tools/freebsd-gcc-9.dds'
    else:
        tc_fpath = ROOT / 'tools/gcc-9.dds'

    self_deps_build(dds_exe, str(tc_fpath), paths.SELF_TEST_REPO_DIR,
                    ROOT / 'remote.dds')
    self_build(dds_exe, toolchain=str(tc_fpath), dds_flags=['--apps'])

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
