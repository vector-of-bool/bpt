from pathlib import Path

from dds_ci import proc

PROJECT_ROOT = Path(__file__).absolute().parent.parent


def self_deps_get(dds_exe: Path, repo_dir: Path) -> None:
    proc.check_run(
        dds_exe,
        'deps',
        'get',
        ('--repo-dir', repo_dir),
        ('--remote-list', PROJECT_ROOT / 'remote.dds'),
    )


if __name__ == "__main__":
    self_deps_get(PROJECT_ROOT / '_build/dds',
                  PROJECT_ROOT / '_build/_self-repo')
