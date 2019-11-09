import argparse
import os
import sys
from pathlib import Path
from typing import Sequence, NamedTuple
import subprocess
import urllib.request

HERE = Path(__file__).parent.absolute()
TOOLS_DIR = HERE
PROJECT_ROOT = HERE.parent
PREBUILT_DDS = PROJECT_ROOT / '_prebuilt/dds'


class CIOptions(NamedTuple):
    cxx: Path
    toolchain: str


def _do_bootstrap_build(opts: CIOptions) -> None:
    print('Bootstrapping by a local build of prior versions...')
    subprocess.check_call([
        sys.executable,
        '-u',
        str(TOOLS_DIR / 'bootstrap.py'),
        f'--cxx={opts.cxx}',
    ])


def _do_bootstrap_download() -> None:
    filename = {
        'win32': 'dds-win-x64.exe',
        'linux': 'dds-linux-x64',
        'darwin': 'dds-macos-x64',
    }.get(sys.platform)
    if filename is None:
        raise RuntimeError(f'We do not have a prebuilt DDS binary for the "{sys.platform}" platform')
    url = f'https://github.com/vector-of-bool/dds/releases/download/bootstrap-p2/{filename}'

    print(f'Downloading prebuilt DDS executable: {url}')
    stream = urllib.request.urlopen(url)
    PREBUILT_DDS.parent.mkdir(exist_ok=True, parents=True)
    with PREBUILT_DDS.open('wb') as fd:
        while True:
            buf = stream.read(1024 * 4)
            if not buf:
                break
            fd.write(buf)

    if os.name != 'nt':
        # Mark the binary executable. By default it won't be
        mode = PREBUILT_DDS.stat().st_mode
        mode |= 0b001_001_001
        PREBUILT_DDS.chmod(mode)


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-B',
        '--bootstrap-with',
        help=
        'Skip the prebuild-bootstrap step. This requires a _prebuilt/dds to exist!',
        choices=('download', 'build'),
        required=True,
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

    opts = CIOptions(cxx=Path(args.cxx), toolchain=args.toolchain)

    if args.bootstrap_with == 'build':
        _do_bootstrap_build(opts)
    elif args.bootstrap_with == 'download':
        _do_bootstrap_download()
    else:
        assert False, 'impossible'

    subprocess.check_call([
        str(PREBUILT_DDS),
        'deps',
        'build',
        f'-T{opts.toolchain}',
        f'--repo-dir={PROJECT_ROOT / "external/repo"}',
    ])

    subprocess.check_call([
        str(PREBUILT_DDS),
        'build',
        '--full',
        f'-T{opts.toolchain}',
    ])

    subprocess.check_call([
        sys.executable,
        '-u',
        str(TOOLS_DIR / 'test.py'),
        f'--exe={PROJECT_ROOT / "_build/dds"}',
        f'-T{opts.toolchain}',
    ])

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
