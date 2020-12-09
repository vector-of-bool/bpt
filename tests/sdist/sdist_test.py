from tests.dds import DDS, dds_fixture_conf_1


@dds_fixture_conf_1('create')
def test_create_sdist(dds: DDS) -> None:
    dds.sdist_create()
    sd_dir = dds.build_dir / 'foo@1.2.3.tar.gz'
    assert sd_dir.is_file()


@dds_fixture_conf_1('create')
def test_export_sdist(dds: DDS) -> None:
    dds.sdist_export()
    assert (dds.repo_dir / 'foo@1.2.3').is_dir()


@dds_fixture_conf_1('create')
def test_import_sdist_archive(dds: DDS) -> None:
    repo_content_path = dds.repo_dir / 'foo@1.2.3'
    assert not repo_content_path.is_dir()
    dds.sdist_create()
    assert not repo_content_path.is_dir()
    dds.repo_import(dds.build_dir / 'foo@1.2.3.tar.gz')
    assert repo_content_path.is_dir()
    assert repo_content_path.joinpath('library.jsonc').is_file()
    # Excluded file will not be in the sdist:
    assert not repo_content_path.joinpath('other-file.txt').is_file()
