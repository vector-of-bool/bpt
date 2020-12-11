import pytest

from dds_ci.testing import ProjectOpener, Project


@pytest.fixture()
def test_project(project_opener: ProjectOpener) -> Project:
    return project_opener.open('projects/sdist')


def test_create_sdist(test_project: Project) -> None:
    test_project.sdist_create()
    sd_dir = test_project.build_root / 'foo@1.2.3.tar.gz'
    assert sd_dir.is_file()


def test_export_sdist(test_project: Project) -> None:
    test_project.sdist_export()
    assert (test_project.dds.repo_dir / 'foo@1.2.3').is_dir()


def test_import_sdist_archive(test_project: Project) -> None:
    repo_content_path = test_project.dds.repo_dir / 'foo@1.2.3'
    assert not repo_content_path.is_dir()
    test_project.sdist_create()
    assert not repo_content_path.is_dir()
    test_project.dds.repo_import(test_project.build_root / 'foo@1.2.3.tar.gz')
    assert repo_content_path.is_dir()
    assert repo_content_path.joinpath('library.jsonc').is_file()
    # Excluded file will not be in the sdist:
    assert not repo_content_path.joinpath('other-file.txt').is_file()
