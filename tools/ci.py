import argparse
import os
import sys
import pytest
from pathlib import Path
from typing import Sequence, NamedTuple
import subprocess
import urllib.request
import shutil

from self_build import self_build
from self_deps_get import self_deps_get
from self_deps_build import self_deps_build
from dds_ci import paths, proc


class CIOptions(NamedTuple):
    cxx: Path
    toolchain: str
    skip_deps: bool


def _do_bootstrap_build(opts: CIOptions) -> None:
    print('Bootstrapping by a local build of prior versions...')
    subprocess.check_call([
        sys.executable,
        '-u',
        str(paths.TOOLS_DIR / 'bootstrap.py'),
        f'--cxx={opts.cxx}',
    ])


def _do_bootstrap_download() -> None:
    filename = {
        'win32': 'dds-win-x64.exe',
        'linux': 'dds-linux-x64',
        'darwin': 'dds-macos-x64',
    }.get(sys.platform)
    if filename is None:
        raise RuntimeError(f'We do not have a prebuilt DDS binary for '
                           f'the "{sys.platform}" platform')
    url = f'https://github.com/vector-of-bool/dds/releases/download/bootstrap-p4/{filename}'

    print(f'Downloading prebuilt DDS executable: {url}')
    stream = urllib.request.urlopen(url)
    paths.PREBUILT_DDS.parent.mkdir(exist_ok=True, parents=True)
    with paths.PREBUILT_DDS.open('wb') as fd:
        while True:
            buf = stream.read(1024 * 4)
            if not buf:
                break
            fd.write(buf)

    if os.name != 'nt':
        # Mark the binary executable. By default it won't be
        mode = paths.PREBUILT_DDS.stat().st_mode
        mode |= 0b001_001_001
        paths.PREBUILT_DDS.chmod(mode)


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-B',
        '--bootstrap-with',
        help='How are we to obtain a bootstrapped DDS executable?',
        choices=('download', 'build', 'skip'),
        required=True,
    )
    parser.add_argument(
        '--cxx', help='The name/path of the C++ compiler to use.')
    parser.add_argument(
        '--toolchain',
        '-T',
        help='The toolchain to use for the CI process',
        required=True)
    parser.add_argument(
        '--skip-deps',
        action='store_true',
        help='If specified, will skip getting and building '
        'dependencies. (They must already be present)')
    args = parser.parse_args(argv)

    opts = CIOptions(
        cxx=Path(args.cxx or 'unspecified'),
        toolchain=args.toolchain,
        skip_deps=args.skip_deps)

    if args.bootstrap_with == 'build':
        if args.cxx is None:
            raise RuntimeError(
                '`--cxx` must be given when using `--bootstrap-with=build`')
        _do_bootstrap_build(opts)
    elif args.bootstrap_with == 'download':
        _do_bootstrap_download()
    elif args.bootstrap_with == 'skip':
        pass
    else:
        assert False, 'impossible'

    if not opts.skip_deps:
        ci_repo_dir = paths.BUILD_DIR / '_ci-repo'
        if ci_repo_dir.exists():
            shutil.rmtree(ci_repo_dir)
        self_deps_get(paths.PREBUILT_DDS, ci_repo_dir)
        self_deps_build(paths.PREBUILT_DDS, opts.toolchain, ci_repo_dir,
                        paths.PROJECT_ROOT / 'remote.dds')

    self_build(
        paths.PREBUILT_DDS,
        toolchain=opts.toolchain,
        dds_flags=['--warnings', '--tests', '--apps'])
    print('Main build PASSED!')

    self_build(
        paths.CUR_BUILT_DDS,
        toolchain=opts.toolchain,
        dds_flags=['--warnings', '--tests', '--apps'])
    print('Bootstrap test PASSED!')

    return pytest.main([
        '-v',
        '--durations=10',
        f'--basetemp={paths.BUILD_DIR / "_tmp"}',
        '-n4',
        'tests/',
    ])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
