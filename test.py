#!/usr/bin/env python3
import argparse
from pathlib import Path
from typing import List, NamedTuple
import subprocess
import sys


class TestOptions(NamedTuple):
    exe: Path
    toolchain: str


def run_test_dir(dir: Path, opts: TestOptions) -> bool:
    try:
        subprocess.check_call(
            [
                str(opts.exe),
                'build',
                '--export',
                '--warnings',
                '--tests',
                '--toolchain',
                opts.toolchain,
            ],
            cwd=dir,
        )
    except subprocess.CalledProcessError:
        import traceback
        traceback.print_exc()
        return False
    return True


def run_tests(opts: TestOptions) -> int:
    print('Sanity check...')
    subprocess.check_output([str(opts.exe), '--help'])
    tests_subdir = Path(__file__).parent.absolute() / 'tests'

    test_dirs = tests_subdir.glob('*.test')
    ret = 0
    for td in test_dirs:
        if not run_test_dir(td, opts):
            ret = 1
    return ret


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--exe',
        '-e',
        help='Path to the ddslim executable to test',
        required=True)
    parser.add_argument(
        '--toolchain',
        '-T',
        help='The ddslim toolchain to use while testing',
        required=True,
    )
    args = parser.parse_args(argv)

    opts = TestOptions(exe=Path(args.exe).absolute(), toolchain=args.toolchain)

    return run_tests(opts)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
