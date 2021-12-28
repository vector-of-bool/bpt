import pytest
from pathlib import Path
from typing import Tuple
import subprocess
import platform

from dds_ci import proc
from dds_ci.dds import DDSWrapper
from dds_ci.testing import ProjectOpener, Project, error


@pytest.fixture()
def test_project(project_opener: ProjectOpener) -> Project:
    return project_opener.open('projects/simple')


def test_create_pkg(test_project: Project, tmp_path: Path) -> None:
    # Create in the default location
    test_project.pkg_create()
    sd_dir = test_project.build_root / 'foo@1.2.3.tar.gz'
    assert sd_dir.is_file(), 'Did not create an sdist in the default location'
    # Create in a different location
    dest = tmp_path / 'dummy.tar.gz'
    test_project.pkg_create(dest=dest)
    assert dest.is_file(), 'Did not create an sdist in the new location'


def test_create_pkg_already_exists_fails(test_project: Project) -> None:
    test_project.pkg_create()
    with error.expect_error_marker('sdist-already-exists'):
        test_project.pkg_create()


def test_create_pkg_already_exists_fails_with_explicit_fail(test_project: Project) -> None:
    test_project.pkg_create()
    with error.expect_error_marker('sdist-already-exists'):
        test_project.pkg_create(if_exists='fail')


def test_create_pkg_already_exists_succeeds_with_ignore(test_project: Project) -> None:
    test_project.pkg_create()
    test_project.pkg_create(if_exists='ignore')


def test_create_pkg_already_exists_succeeds_with_replace(test_project: Project) -> None:
    sd_path: Path = test_project.build_root / 'foo@1.2.3.tar.gz'
    test_project.build_root.mkdir(exist_ok=True, parents=True)
    sd_path.touch()
    test_project.pkg_create(if_exists='replace')
    assert sd_path.stat().st_size > 0, 'Did not replace the existing file'


@pytest.fixture()
def _test_pkg(test_project: Project) -> Tuple[Path, Project]:
    repo_content_path = test_project.dds.repo_dir / 'foo@1.2.3'
    assert not repo_content_path.is_dir()
    test_project.pkg_create()
    assert not repo_content_path.is_dir()
    return test_project.build_root / 'foo@1.2.3.tar.gz', test_project


def test_import_sdist_archive(_test_pkg: Tuple[Path, Project]) -> None:
    sdist, project = _test_pkg
    repo_content_path = project.dds.repo_dir / 'foo@1.2.3'
    project.dds.pkg_import(sdist)
    assert repo_content_path.is_dir(), \
        'The package did not appear in the local cache'
    # Excluded file will not be in the sdist:
    assert not repo_content_path.joinpath('other-file.txt').is_file(), \
        'Non-package content appeared in the package cache'


@pytest.mark.skipif(platform.system() == 'Windows',
                    reason='Windows has trouble reading packages from stdin. Need to investigate.')
def test_import_sdist_stdin(_test_pkg: Tuple[Path, Project]) -> None:
    sdist, project = _test_pkg
    pipe = subprocess.Popen(
        list(proc.flatten_cmd([
            project.dds.path,
            project.dds.cache_dir_arg,
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
    _check_import(project.dds.repo_dir / 'foo@1.2.3')


def test_import_sdist_dir(test_project: Project) -> None:
    test_project.dds.run(['pkg', 'import', test_project.dds.cache_dir_arg, test_project.root])
    _check_import(test_project.dds.repo_dir / 'foo@1.2.3')


def _check_import(repo_content_path: Path) -> None:
    assert repo_content_path.is_dir(), \
        'The package did not appear in the local cache'
    # Excluded file will not be in the sdist:
    assert not repo_content_path.joinpath('other-file.txt').is_file(), \
        'Non-package content appeared in the package cache'


def test_sdist_invalid_project(tmp_project: Project) -> None:
    with error.expect_error_marker('no-package-json5'):
        tmp_project.pkg_create()


@pytest.mark.skipif(platform.system() != 'Linux', reason='We know this fails on Linux')
def test_sdist_unreadable_dir(dds: DDSWrapper) -> None:
    with error.expect_error_marker('failed-package-json5-scan'):
        dds.run(['pkg', 'create', '--project=/root'])


def test_sdist_invalid_json5(tmp_project: Project) -> None:
    tmp_project.write('package.json5', 'bogus json5')
    with error.expect_error_marker('package-json5-parse-error'):
        tmp_project.pkg_create()
