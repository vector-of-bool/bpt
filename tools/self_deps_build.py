import argparse
from pathlib import Path

from dds_ci import cli, proc, paths


def self_deps_build(exe: Path, toolchain: str, repo_dir: Path,
                    remote_list: Path) -> None:
    proc.check_run(
        exe,
        'deps',
        'build',
        ('--repo-dir', repo_dir),
        ('-T', toolchain),
    )


def main():
    parser = argparse.ArgumentParser()
    cli.add_dds_exe_arg(parser)
    cli.add_tc_arg(parser)
    parser.add_argument('--repo-dir', default=paths.SELF_TEST_REPO_DIR)
    args = parser.parse_args()
    self_deps_build(
        Path(args.exe), args.toolchain, args.repo_dir,
        paths.PROJECT_ROOT / 'remote.dds')


if __name__ == "__main__":
    main()
