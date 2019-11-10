#!/usr/bin/env python3
import argparse
from pathlib import Path
from typing import List, NamedTuple
import shutil
import subprocess
import sys

ROOT = Path(__file__).parent.parent.absolute()


def bootstrap_self(exe: Path, toolchain: str):
    # Copy the exe to another location, as windows refuses to let a binary be
    # replaced while it is executing
    new_exe = ROOT / '_dds.bootstrap-test.exe'
    shutil.copy2(exe, new_exe)
    res = subprocess.run([str(new_exe), 'build', f'-FT{toolchain}'])
    new_exe.unlink()
    if res.returncode != 0:
        raise RuntimeError('The bootstrap test failed!')
    print('Bootstrap test PASSED!')


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--exe',
        '-e',
        help='Path to the dds executable to test',
        required=True)
    parser.add_argument(
        '--toolchain',
        '-T',
        help='The dds toolchain to use while testing',
        required=True,
    )
    args = parser.parse_args(argv)
    bootstrap_self(Path(args.exe), args.toolchain)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
