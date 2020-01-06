import argparse
from pathlib import Path
import subprocess
import os
from typing import Sequence, NamedTuple
import sys
import shutil


class BootstrapPhase(NamedTuple):
    ref: str
    nix_compiler: str
    win_compiler: str

    @property
    def platform_compiler(self):
        if os.name == 'nt':
            return self.win_compiler
        else:
            return self.nix_compiler


BOOTSTRAP_PHASES = [
    BootstrapPhase('bootstrap-p1', 'g++-8', 'cl.exe'),
    BootstrapPhase('bootstrap-p4', 'g++-8', 'cl.exe'),
    BootstrapPhase('bootstrap-p5.1', 'g++-9', 'cl.exe'),
]

HERE = Path(__file__).parent.absolute()
PROJECT_ROOT = HERE.parent
BUILD_DIR = PROJECT_ROOT / '_build'
BOOTSTRAP_BASE_DIR = BUILD_DIR / '_bootstrap'
PREBUILT_DIR = PROJECT_ROOT / '_prebuilt'

EXE_SUFFIX = '.exe' if os.name == 'nt' else ''


def _run_quiet(cmd, **kwargs) -> None:
    cmd = [str(s) for s in cmd]
    res = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        **kwargs,
    )
    if res.returncode != 0:
        print(f'Subprocess command {cmd} failed '
              f'[{res.returncode}]:\n{res.stdout.decode()}')
        raise subprocess.CalledProcessError(res.returncode, cmd)


def _clone_bootstrap_phase(ref: str) -> Path:
    print(f'Clone revision: {ref}')
    bts_dir = BOOTSTRAP_BASE_DIR / ref
    if bts_dir.exists():
        shutil.rmtree(bts_dir)
    _run_quiet([
        'git',
        'clone',
        '--depth=1',
        f'--branch={ref}',
        f'file://{PROJECT_ROOT}',
        bts_dir,
    ])
    return bts_dir


def _build_bootstrap_phase(ph: BootstrapPhase, bts_dir: Path) -> None:
    print(f'Build revision: {ph.ref} [This may take a moment]')
    env = os.environ.copy()
    env['DDS_BOOTSTRAP_PREV_EXE'] = str(PREBUILT_DIR / F'dds{EXE_SUFFIX}')
    _run_quiet(
        [
            sys.executable,
            '-u',
            str(bts_dir / 'tools/build.py'),
            f'--cxx={ph.platform_compiler}',
        ],
        env=env,
        cwd=bts_dir,
    )


def _pull_executable(bts_dir: Path) -> Path:
    prebuild_dir = (PROJECT_ROOT / '_prebuilt')
    prebuild_dir.mkdir(exist_ok=True)
    generated = list(bts_dir.glob(f'_build/dds{EXE_SUFFIX}'))
    assert len(generated) == 1, repr(generated)
    exe, = generated
    dest = prebuild_dir / exe.name
    if dest.exists():
        dest.unlink()
    exe.rename(dest)
    return dest


def _run_boot_phase(phase: BootstrapPhase) -> Path:
    bts_dir = _clone_bootstrap_phase(phase.ref)
    _build_bootstrap_phase(phase, bts_dir)
    return _pull_executable(bts_dir)


def main(argv: Sequence[str]) -> int:
    for idx, phase in enumerate(BOOTSTRAP_PHASES):
        print(f'Bootstrap phase [{idx+1}/{len(BOOTSTRAP_PHASES)}]')
        exe = _run_boot_phase(phase)

    print(f'A bootstrapped DDS executable has been generated: {exe}')
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
