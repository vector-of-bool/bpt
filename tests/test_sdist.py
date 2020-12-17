import pytest
from pathlib import Path
from typing import Tuple
import subprocess

from dds_ci import proc
from dds_ci.testing import ProjectOpener, Project


@pytest.fixture()
def test_project(project_opener: ProjectOpener) -> Project:
    return project_opener.open('projects/sdist')


def test_create_sdist(test_project: Project, tmp_path: Path) -> None:
    # Create in the default location
    test_project.sdist_create()
    sd_dir = test_project.build_root / 'foo@1.2.3.tar.gz'
    assert sd_dir.is_file(), 'Did not create an sdist in the default location'
    # Create in a different location
    dest = tmp_path / 'dummy.tar.gz'
    test_project.sdist_create(dest=dest)
    assert dest.is_file(), 'Did not create an sdist in the new location'


@pytest.fixture()
def test_sdist(test_project: Project) -> Tuple[Path, Project]:
    repo_content_path = test_project.dds.repo_dir / 'foo@1.2.3'
    assert not repo_content_path.is_dir()
    test_project.sdist_create()
    assert not repo_content_path.is_dir()
    return test_project.build_root / 'foo@1.2.3.tar.gz', test_project


def test_import_sdist_archive(test_sdist: Tuple[Path, Project]) -> None:
    sdist, project = test_sdist
    repo_content_path = project.dds.repo_dir / 'foo@1.2.3'
    project.dds.pkg_import(sdist)
    assert repo_content_path.is_dir(), \
        'The package did not appear in the local cache'
    assert repo_content_path.joinpath('library.jsonc').is_file(), \
        'The package\'s library.jsonc did not get imported'
    # Excluded file will not be in the sdist:
    assert not repo_content_path.joinpath('other-file.txt').is_file(), \
        'Non-package content appeared in the package cache'


def test_import_sdist_stdin(test_sdist: Tuple[Path, Project]) -> None:
    sdist, project = test_sdist
    repo_content_path = project.dds.repo_dir / 'foo@1.2.3'
    pipe = subprocess.Popen(
        list(proc.flatten_cmd([
            project.dds.path,
            project.dds.repo_dir_arg,
            'pkg',
            'import',
            '--stdin',
        ])),
        stdin=subprocess.PIPE,
    )
    assert pipe.stdin
    with sdist.open('rb') as sdist_bin:
        buf = sdist_bin.read(1024)
        while buf:
            pipe.stdin.write(buf)
            buf = sdist_bin.read(1024)
        pipe.stdin.close()

    rc = pipe.wait()
    assert rc == 0, 'Subprocess failed'
    # project.dds.pkg_import(sdist)
    assert repo_content_path.is_dir(), \
        'The package did not appear in the local cache'
    assert repo_content_path.joinpath('library.jsonc').is_file(), \
        'The package\'s library.jsonc did not get imported'
    # Excluded file will not be in the sdist:
    assert not repo_content_path.joinpath('other-file.txt').is_file(), \
        'Non-package content appeared in the package cache'
