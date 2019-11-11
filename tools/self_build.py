#!/usr/bin/env python3
import argparse
from pathlib import Path
from typing import List, NamedTuple
import shutil
import subprocess
import sys

from dds_ci import cli, proc

ROOT = Path(__file__).parent.parent.absolute()


def self_build(exe: Path, *, toolchain: str, lmi_path: Path = None):
    # Copy the exe to another location, as windows refuses to let a binary be
    # replaced while it is executing
    new_exe = ROOT / '_dds.bootstrap-test.exe'
    shutil.copy2(exe, new_exe)
    try:
        proc.check_run(
            new_exe,
            'build',
            '--full',
            ('--toolchain', toolchain),
            ('-I', lmi_path) if lmi_path else (),
        )
    finally:
        new_exe.unlink()


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser()
    cli.add_tc_arg(parser)
    cli.add_dds_exe_arg(parser)
    args = parser.parse_args(argv)
    self_build(Path(args.exe), args.toolchain)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
