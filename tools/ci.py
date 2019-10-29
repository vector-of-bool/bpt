import argparse
import sys
from pathlib import Path
from typing import Sequence, NamedTuple
import subprocess

HERE = Path(__file__).parent.absolute()
TOOLS_DIR = HERE
PROJECT_ROOT = HERE.parent
PREBUILT_DDS = PROJECT_ROOT / '_prebuilt/dds'


class CIOptions(NamedTuple):
    skip_bootstrap: bool
    cxx: Path
    toolchain: str


def _do_bootstrap(opts: CIOptions) -> None:
    print('Running bootstrap')
    subprocess.check_call([
        sys.executable,
        TOOLS_DIR / 'bootstrap.py',
        f'--cxx={opts.cxx}',
    ])


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--skip-bootstrap',
        action='store_true',
        help=
        'Skip the prebuild-bootstrap step. This requires a _prebuilt/dds to exist!',
    )
    parser.add_argument(
        '--cxx',
        help='The name/path of the C++ compiler to use.',
        required=True)
    parser.add_argument(
        '--toolchain',
        '-T',
        help='The toolchain to use for the CI process',
        required=True)
    args = parser.parse_args(argv)
    opts = CIOptions(
        skip_bootstrap=args.skip_bootstrap,
        cxx=Path(args.cxx),
        toolchain=args.toolchain)
    if not opts.skip_bootstrap:
        _do_bootstrap(opts)

    subprocess.check_call([
        PREBUILT_DDS,
        'deps',
        'build',
        f'-T{opts.toolchain}',
        f'--repo-dir={PROJECT_ROOT / "external/repo"}',
    ])

    subprocess.check_call([
        PREBUILT_DDS,
        'build',
        '--full',
        f'-T{opts.toolchain}',
    ])

    subprocess.check_call([
        sys.executable,
        TOOLS_DIR / 'test.py',
        f'--exe={PROJECT_ROOT / "_build/dds"}',
        f'-T{opts.toolchain}',
    ])

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
