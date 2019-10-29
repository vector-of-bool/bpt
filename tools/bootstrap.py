import argparse
from pathlib import Path
import subprocess
import os
from typing import Sequence
import sys
import shutil

BOOTSTRAP_PHASES = [
    'bootstrap-p1',
    'bootstrap-p2',
]

HERE = Path(__file__).parent.absolute()
PROJECT_ROOT = HERE.parent
BUILD_DIR = PROJECT_ROOT / '_build'
BOOTSTRAP_DIR = BUILD_DIR / '_bootstrap'
PREBUILT_DIR = PROJECT_ROOT / '_prebuilt'


def _run_quiet(args) -> None:
    cmd = [str(s) for s in args]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if res.returncode != 0:
        print(f'Subprocess command {cmd} failed '
              f'[{res.returncode}]:\n{res.stdout.decode()}')
        raise subprocess.CalledProcessError(res.returncode, cmd)


def _clone_bootstrap_phase(ph: str) -> None:
    print(f'Cloning: {ph}')
    if BOOTSTRAP_DIR.exists():
        shutil.rmtree(BOOTSTRAP_DIR)
    _run_quiet([
        'git',
        'clone',
        '--depth=1',
        f'--branch={ph}',
        f'file://{PROJECT_ROOT}',
        BOOTSTRAP_DIR,
    ])


def _build_bootstrap_phase(ph: str, args: argparse.Namespace) -> None:
    print(f'Running build: {ph} (Please wait a moment...)')
    env = os.environ.copy()
    env['DDS_BOOTSTRAP_PREV_EXE'] = PREBUILT_DIR / 'dds'
    subprocess.check_call(
        [
            sys.executable,
            str(BOOTSTRAP_DIR / 'tools/build.py'),
            f'--cxx={args.cxx}',
        ],
        env=env,
    )


def _pull_executable() -> Path:
    prebuild_dir = (PROJECT_ROOT / '_prebuilt')
    prebuild_dir.mkdir(exist_ok=True)
    exe, = list(BOOTSTRAP_DIR.glob('_build/dds*'))
    dest = prebuild_dir / exe.name
    exe.rename(dest)
    return dest


def _run_boot_phase(phase: str, args: argparse.Namespace) -> Path:
    _clone_bootstrap_phase(phase)
    _build_bootstrap_phase(phase, args)
    return _pull_executable()


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--cxx', help='The C++ compiler to use for the build', required=True)
    args = parser.parse_args(argv)
    for phase in BOOTSTRAP_PHASES:
        exe = _run_boot_phase(phase, args)

    print(f'A bootstrapped DDS executable has been generated: {exe}')

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
