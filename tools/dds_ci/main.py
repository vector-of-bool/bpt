import argparse
from contextlib import contextmanager
import multiprocessing
import pytest
from pathlib import Path
from concurrent import futures
import shutil
import sys
from typing import Iterator, NoReturn, Sequence, Optional, cast
from typing_extensions import Protocol
import subprocess

from . import paths, toolchain
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
    parser.add_argument('--compile-file', help='Compile the given file', type=Path, default=None)
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
    parser.add_argument('--clean', action='store_true', help="Remove prior build/deps results before building")
    parser.add_argument('--no-test',
                        action='store_false',
                        dest='do_test',
                        help='Skip testing and just build the final result')
    parser.add_argument('--no-unit-tests',
                        action='store_false',
                        dest='do_unit_test',
                        help='Skip building and running unit tests')
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
    #: Compile just the one named file
    compile_file: Optional[Path]
    #: Build+run unit tests
    do_unit_test: bool


def parse_argv(argv: Sequence[str]) -> CommandArguments:
    """Parse the given dds-ci command-line argument list"""
    return cast(CommandArguments, make_argparser().parse_args(argv))


def test_build(dds: DDSWrapper, args: CommandArguments) -> DDSWrapper:
    """
    Execute the build that generates the test-mode executable. Uses the given 'dds'
    to build the new dds. Returns a DDSWrapper around the generated test executable.
    """
    test_tc = args.test_toolchain or toolchain.get_default_audit_toolchain()
    print(f'Test build is building with toolchain: {test_tc}')
    build_dir = paths.BUILD_DIR
    with toolchain.fixup_toolchain(test_tc) as new_tc:
        dds.build(toolchain=new_tc,
                  root=paths.PROJECT_ROOT,
                  build_root=build_dir,
                  jobs=args.jobs,
                  tweaks_dir=paths.TWEAKS_DIR,
                  timeout=60 * 15)
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
        f'--junit-xml={paths.BUILD_DIR}/pytest-junit.xml',
        str(paths.PROJECT_ROOT / 'tests/'),
    ])


@contextmanager
def fixup_toolchain(args: CommandArguments) -> Iterator[Path]:
    use_dev = args.rapid or args.compile_file
    main_tc = args.toolchain or (
        # If we are in rapid-dev mode, use the test toolchain, which had audit/debug enabled
        toolchain.get_default_toolchain() if not use_dev else toolchain.get_default_audit_toolchain())
    with toolchain.fixup_toolchain(main_tc) as new_tc:
        yield new_tc


def main_build(dds: DDSWrapper, args: CommandArguments) -> int:
    """
    Execute the main build of dds using the given 'dds' executable to build itself.
    """
    with fixup_toolchain(args) as new_tc:
        try:
            dds.build(toolchain=new_tc,
                      root=paths.PROJECT_ROOT,
                      build_root=paths.BUILD_DIR,
                      tweaks_dir=paths.TWEAKS_DIR,
                      with_tests=args.do_unit_test,
                      jobs=args.jobs,
                      timeout=60 * 15)
        except subprocess.CalledProcessError as e:
            if args.rapid:
                return e.returncode
            raise
    return 0


def ci_with_dds(dds: DDSWrapper, args: CommandArguments) -> int:
    """
    Execute CI using the given prior 'dds' executable.
    """
    if args.compile_file:
        with fixup_toolchain(args) as tc:
            try:
                dds.compile_file([args.compile_file],
                                 toolchain=tc,
                                 tweaks_dir=paths.TWEAKS_DIR,
                                 project_dir=paths.PROJECT_ROOT,
                                 out=paths.BUILD_DIR)
            except subprocess.CalledProcessError as err:
                return err.returncode
            return 0

    if args.clean:
        dds.clean(build_dir=paths.BUILD_DIR)

    dds.run(['pkg', 'repo', 'add', 'https://repo-1.dds.pizza'])

    if args.rapid:
        return main_build(dds, args)

    pool = futures.ThreadPoolExecutor()
    test_fut = pool.submit(lambda: 0)
    if args.do_test:
        # Build the test executable:
        test_dds = test_build(dds, args)
        # Move the generated exe and start tests. We'll start building the main
        # EXE and don't want to overwrite the test one while the tests are running
        dds_cp = paths.BUILD_DIR / ('dds.test' + paths.EXE_SUFFIX)
        test_dds.path.rename(dds_cp)
        test_dds.path = dds_cp
        # Workaround: dds doesn't rebuild the test-driver on toolchain changes:
        shutil.rmtree(paths.BUILD_DIR / '_test-driver')
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


def start() -> NoReturn:
    sys.exit(main(sys.argv[1:]))


if __name__ == "__main__":
    start()
