import pytest
from pathlib import Path
from typing import Tuple
import platform

from dds_ci.dds import DDSWrapper
from dds_ci.testing import ProjectOpener, Project, error, CRSRepo


@pytest.fixture()
def test_project(project_opener: ProjectOpener) -> Project:
    return project_opener.open('projects/simple')


def test_create_pkg(test_project: Project, tmp_path: Path) -> None:
    # Create in the default location
    test_project.pkg_create()
    sd_dir = test_project.build_root / 'foo@1.2.3~1.tar.gz'
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
    sd_path: Path = test_project.build_root / 'foo@1.2.3~1.tar.gz'
    test_project.build_root.mkdir(exist_ok=True, parents=True)
    sd_path.touch()
    test_project.pkg_create(if_exists='replace')
    assert sd_path.stat().st_size > 0, 'Did not replace the existing file'


@pytest.fixture()
def _test_pkg(test_project: Project) -> Tuple[Path, Project]:
    repo_content_path = test_project.dds.repo_dir / 'foo@1.2.3~1'
    assert not repo_content_path.is_dir()
    test_project.pkg_create()
    assert not repo_content_path.is_dir()
    return test_project.build_root / 'foo@1.2.3~1.tar.gz', test_project


def test_sdist_invalid_project(tmp_project: Project) -> None:
    with error.expect_error_marker('no-pkg-meta-files'):
        tmp_project.pkg_create()


@pytest.mark.skipif(platform.system() != 'Linux', reason='We know this fails on Linux')
def test_sdist_unreadable_dir(dds: DDSWrapper) -> None:
    with error.expect_error_marker('sdist-open-fail-generic'):
        dds.run(['pkg', 'create', '--project=/root'])


def test_sdist_invalid_json5(tmp_project: Project) -> None:
    tmp_project.write('project.json5', 'bogus json5')
    with error.expect_error_marker('package-json5-parse-error'):
        tmp_project.pkg_create()


def test_pkg_search(tmp_crs_repo: CRSRepo, tmp_project: Project) -> None:
    with error.expect_error_marker('pkg-search-no-result'):
        tmp_project.dds.run(['pkg', 'search', 'test-pkg', '-r', tmp_crs_repo.path])
    tmp_project.project_json = {
        'name': 'test-pkg',
        'version': '0.1.2',
    }
    tmp_crs_repo.import_(tmp_project.root)
    # No error now:
    tmp_project.dds.run(['pkg', 'search', 'test-pkg', '-r', tmp_crs_repo.path])
