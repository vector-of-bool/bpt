import argparse
import json
from contextlib import contextmanager
import enum
import multiprocessing
import pytest
from pathlib import Path
from concurrent import futures
import sys
import os
from typing import NoReturn, Sequence, Optional, Iterator
from typing_extensions import Protocol
import subprocess

import json5

from . import paths
from .dds import DDSWrapper
from .bootstrap import BootstrapMode, get_bootstrap_exe


def make_argparser() -> argparse.ArgumentParser:
    """Create an argument parser for the dds-ci command-line"""
    parser = argparse.ArgumentParser()
    parser.add_argument('-B',
                        '--bootstrap-with',
                        help='How are we to obtain a bootstrapped DDS executable?',
                        metavar='{download,build,skip,lazy}',
                        type=BootstrapMode,
                        default=BootstrapMode.Lazy)
    parser.add_argument('--rapid', help='Run CI for fast development iterations', action='store_true')
    parser.add_argument('--test-toolchain',
                        '-TT',
                        type=Path,
                        metavar='<toolchain-file>',
                        help='The toolchain to use for the first build, which will be passed through the tests')
    parser.add_argument('--main-toolchain',
                        '-T',
                        type=Path,
                        dest='toolchain',
                        metavar='<toolchain-file>',
                        help='The toolchain to use for the final build')
    parser.add_argument('--jobs',
                        '-j',
                        type=int,
                        help='Number of parallel jobs to use when building and testing',
                        default=multiprocessing.cpu_count() + 2)
    parser.add_argument('--build-only', action='store_true', help='Only build the dds executable, do not run tests')
    parser.add_argument('--clean', action='store_true', help="Don't remove prior build/deps results")
    parser.add_argument('--no-test',
                        action='store_false',
                        dest='do_test',
                        help='Skip testing and just build the final result')
    return parser


class CommandArguments(Protocol):
    """
    The result of parsing argv with the dds-ci argument parser.
    """
    #: Whether the user wants us to clean result before building
    clean: bool
    #: The bootstrap method the user has requested
    bootstrap_with: BootstrapMode
    #: The toolchain to use when building the 'dds' executable that will be tested.
    test_toolchain: Optional[Path]
    #: The toolchain to use when building the main 'dds' executable to publish
    toolchain: Optional[Path]
    #: The maximum number of parallel jobs for build and test
    jobs: int
    #: Whether we should run the pytest tests
    do_test: bool
    #: Rapid-CI is for 'dds' development purposes
    rapid: bool


def parse_argv(argv: Sequence[str]) -> CommandArguments:
    """Parse the given dds-ci command-line argument list"""
    return make_argparser().parse_args(argv)


@contextmanager
def fixup_toolchain(json_file: Path) -> Iterator[Path]:
    """
    Augment the toolchain at the given path by adding 'ccache' or -fuse-ld=lld,
    if those tools are available on the system. Yields a new toolchain file
    based on 'json_file'
    """
    data = json5.loads(json_file.read_text())
    # Check if we can add ccache
    ccache = paths.find_exe('ccache')
    if ccache:
        print('Found ccache:', ccache)
        data['compiler_launcher'] = [str(ccache)]
    # Check for lld for use with GCC/Clang
    if paths.find_exe('ld.lld') and data.get('compiler_id') in ('gnu', 'clang'):
        print('Linking with `-fuse-ld=lld`')
        data.setdefault('link_flags', []).append('-fuse-ld=lld')
    # Save the new toolchain data
    with paths.new_tempdir() as tdir:
        new_json = tdir / json_file.name
        new_json.write_text(json.dumps(data))
        yield new_json


def get_default_test_toolchain() -> Path:
    """
    Get the default toolchain that should be used for dev and test based on the
    host platform.
    """
    if sys.platform == 'win32':
        return paths.TOOLS_DIR / 'msvc-audit.jsonc'
    elif sys.platform in 'linux':
        return paths.TOOLS_DIR / 'gcc-9-audit.jsonc'
    elif sys.platform == 'darwin':
        return paths.TOOLS_DIR / 'gcc-9-audit-macos.jsonc'
    else:
        raise RuntimeError(f'Unable to determine the default toolchain (sys.platform is {sys.platform!r})')


def get_default_toolchain() -> Path:
    """
    Get the default toolchain that should be used to generate the release executable
    based on the host platform.
    """
    if sys.platform == 'win32':
        return paths.TOOLS_DIR / 'msvc-rel.jsonc'
    elif sys.platform == 'linux':
        return paths.TOOLS_DIR / 'gcc-9-rel.jsonc'
    elif sys.platform == 'darwin':
        return paths.TOOLS_DIR / 'gcc-9-rel-macos.jsonc'
    else:
        raise RuntimeError(f'Unable to determine the default toolchain (sys.platform is {sys.platform!r})')


def test_build(dds: DDSWrapper, args: CommandArguments) -> DDSWrapper:
    """
    Execute the build that generates the test-mode executable. Uses the given 'dds'
    to build the new dds. Returns a DDSWrapper around the generated test executable.
    """
    test_tc = args.test_toolchain or get_default_test_toolchain()
    build_dir = paths.BUILD_DIR / '_ci-test'
    with fixup_toolchain(test_tc) as new_tc:
        dds.build(toolchain=new_tc, root=paths.PROJECT_ROOT, build_root=build_dir, jobs=args.jobs)
    return DDSWrapper(build_dir / ('dds' + paths.EXE_SUFFIX))


def run_pytest(dds: DDSWrapper, args: CommandArguments) -> int:
    """
    Execute pytest, testing against the given 'test_dds' executable. Returns
    the exit code of pytest.
    """
    basetemp = Path('/tmp/dds-ci')
    basetemp.mkdir(exist_ok=True, parents=True)
    return pytest.main([
        '-v',
        '--durations=10',
        '-n',
        str(args.jobs),
        f'--basetemp={basetemp}',
        f'--dds-exe={dds.path}',
        str(paths.PROJECT_ROOT / 'tests/'),
    ])


def main_build(dds: DDSWrapper, args: CommandArguments) -> int:
    """
    Execute the main build of dds using the given 'dds' executable to build itself.
    """
    main_tc = args.toolchain or (
        # If we are in rapid-dev mode, use the test toolchain, which had audit/debug enabled
        get_default_toolchain() if not args.rapid else get_default_test_toolchain())
    with fixup_toolchain(main_tc) as new_tc:
        try:
            dds.build(toolchain=new_tc, root=paths.PROJECT_ROOT, build_root=paths.BUILD_DIR, jobs=args.jobs)
        except subprocess.CalledProcessError as e:
            if args.rapid:
                return e.returncode
            raise
    return 0


def ci_with_dds(dds: DDSWrapper, args: CommandArguments) -> int:
    """
    Execute CI using the given prior 'dds' executable.
    """
    if args.clean:
        dds.clean(build_dir=paths.BUILD_DIR)

    dds.catalog_json_import(paths.PROJECT_ROOT / 'old-catalog.json')

    pool = futures.ThreadPoolExecutor()
    test_fut = pool.submit(lambda: 0)
    if args.do_test and not args.rapid:
        test_dds = test_build(dds, args)
        test_fut = pool.submit(lambda: run_pytest(test_dds, args))

    main_fut = pool.submit(lambda: main_build(dds, args))
    for fut in futures.as_completed({test_fut, main_fut}):
        if fut.result():
            return fut.result()
    return 0


def main(argv: Sequence[str]) -> int:
    args = parse_argv(argv)
    with get_bootstrap_exe(args.bootstrap_with) as f:
        return ci_with_dds(f, args)


def start():
    sys.exit(main(sys.argv[1:]))


if __name__ == "__main__":
    start()
