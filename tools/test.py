#!/usr/bin/env python3
import argparse
from pathlib import Path
from typing import List, NamedTuple
import shutil
import subprocess
import sys

ROOT = Path(__file__).parent.parent.absolute()


class TestOptions(NamedTuple):
    exe: Path
    toolchain: str


def run_test_dir(dir: Path, opts: TestOptions) -> bool:
    fails = 0
    fails += _run_subproc_test(
        dir,
        opts,
        'Full Build',
        'build',
        '--full',
        f'--toolchain={opts.toolchain}',
        f'--export-name={dir.stem}',
    )
    fails += _run_subproc_test(
        dir,
        opts,
        'Source Distribution',
        'sdist',
        f'--out={dir.stem}/test.dsd',
        '--force',
    )
    return fails == 0


def _run_subproc_test(dir: Path, opts: TestOptions, name: str,
                      *args: str) -> int:
    print(f'Running test: {dir.stem} - {name} ', end='')
    out_dir = dir / '_build'
    if out_dir.exists():
        shutil.rmtree(out_dir)
    res = subprocess.run(
        [
            str(opts.exe),
        ] + list(str(s) for s in args),
        cwd=dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if res.returncode != 0:
        print('- FAILED')
        print(f'Test failed with exit code '
              f'[{res.returncode}]:\n{res.stdout.decode()}')
        return 1
    print('- PASSED')
    return 0


def _run_build_test(dir: Path, opts: TestOptions) -> int:
    print(f'Running test: {dir.stem} - build', end='')
    out_dir = dir / '_build'
    if out_dir.exists():
        shutil.rmtree(out_dir)
    res = subprocess.run(
        [
            str(opts.exe),
            'build',
            '--export',
            '--warnings',
            '--tests',
            '--full',
            f'--toolchain={opts.toolchain}',
            f'--out={out_dir}',
            f'--export-name={dir.stem}',
        ],
        cwd=dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if res.returncode != 0:
        print('- FAILED')
        print(f'Test failed with exit code '
              f'[{res.returncode}]:\n{res.stdout.decode()}')
        return 1
    print('- PASSED')
    return 0


def run_tests(opts: TestOptions) -> int:
    print('Sanity check...')
    subprocess.check_output([str(opts.exe), '--help'])
    tests_subdir = ROOT / 'tests'

    test_dirs = tests_subdir.glob('*.test')
    ret = 0
    for td in test_dirs:
        if not run_test_dir(td, opts):
            ret = 1
    return ret


def bootstrap_self(opts: TestOptions):
    # Copy the exe to another location, as windows refuses to let a binary be
    # replaced while it is executing
    new_exe = ROOT / '_ddslime.bootstrap-test.exe'
    shutil.copy2(opts.exe, new_exe)
    res = subprocess.run([
        str(new_exe),
        'build',
        f'-FT{opts.toolchain}',
    ])
    new_exe.unlink()
    if res.returncode != 0:
        print('The bootstrap test failed!', file=sys.stderr)
        return False
    print('Bootstrap test PASSED!')
    return True


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
    parser.add_argument(
        '--skip-bootstrap-test',
        action='store_true',
        help='Skip the self-bootstrap test',
    )
    args = parser.parse_args(argv)

    opts = TestOptions(exe=Path(args.exe).absolute(), toolchain=args.toolchain)

    if not args.skip_bootstrap_test and not bootstrap_self(opts):
        return 2

    return run_tests(opts)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
