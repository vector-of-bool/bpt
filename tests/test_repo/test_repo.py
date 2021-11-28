from pathlib import Path
from dds_ci.testing.fixtures import tmp_project

import tarfile
import pytest
import json
import sqlite3

from dds_ci.dds import DDSWrapper
from dds_ci.paths import PROJECT_ROOT
from dds_ci.testing import Project
from dds_ci.testing.error import expect_error_marker


def test_repo_init(dds: DDSWrapper, tmp_path: Path) -> None:
    dds.run(['repo', 'init', tmp_path, f'--name=testing'])
    assert tmp_path.joinpath('repo.db').is_file()
    assert tmp_path.joinpath('repo.db.gz').is_file()


@pytest.fixture()
def empty_repo(dds: DDSWrapper, tmp_path: Path) -> Path:
    dds.run(['repo', 'init', tmp_path, '--name=testing'])
    assert tmp_path.joinpath('repo.db').is_file()
    assert tmp_path.joinpath('repo.db.gz').is_file()
    return tmp_path


def test_repo_init_already(dds: DDSWrapper, empty_repo: Path) -> None:
    with expect_error_marker('repo-init-already-init'):
        dds.run(['repo', 'init', empty_repo, '--name=testing'])


def test_repo_init_ignore(dds: DDSWrapper, empty_repo: Path) -> None:
    before_time = empty_repo.joinpath('repo.db').stat().st_mtime
    dds.run(['repo', 'init', empty_repo, '--name=testing', '--if-exists=ignore'])
    after_time = empty_repo.joinpath('repo.db').stat().st_mtime
    assert before_time == after_time


def test_repo_init_replace(dds: DDSWrapper, empty_repo: Path) -> None:
    before_time = empty_repo.joinpath('repo.db').stat().st_mtime
    dds.run(['repo', 'init', empty_repo, '--name=testing', '--if-exists=replace'])
    after_time = empty_repo.joinpath('repo.db').stat().st_mtime
    assert before_time < after_time


def test_repo_import(dds: DDSWrapper, empty_repo: Path, tmp_project: Project) -> None:
    tmp_project.write(
        'pkg.json',
        json.dumps({
            'crs_version': 1,
            'name': 'meow',
            'namespace': 'test',
            'version': '1.2.3',
            'meta_version': 1,
            'depends': [],
            'libraries': [{
                'name': 'test',
                'path': '.',
                'uses': [],
            }],
        }))
    dds.run(['repo', 'import', empty_repo, tmp_project.root])


def test_repo_import1(dds: DDSWrapper, empty_repo: Path) -> None:
    dds.run(['repo', 'import', empty_repo, PROJECT_ROOT / 'data/simple.crs'])
    with tarfile.open(empty_repo / 'pkg/test-pkg/1.2.43~1/pkg.tgz') as tf:
        names = tf.getnames()
        assert 'src/my-file.cpp' in names
        assert 'include/my-header.hpp' in names


def test_repo_import2(dds: DDSWrapper, empty_repo: Path) -> None:
    dds.run(['repo', 'import', empty_repo, PROJECT_ROOT / 'data/simple2.crs'])
    with tarfile.open(empty_repo / 'pkg/test-pkg/1.3.0~1/pkg.tgz') as tf:
        names = tf.getnames()
        assert 'include/my-header.hpp' in names


def test_repo_import3(dds: DDSWrapper, empty_repo: Path) -> None:
    dds.run(['repo', 'import', empty_repo, PROJECT_ROOT / 'data/simple3.crs'])
    with tarfile.open(empty_repo / 'pkg/test-pkg/1.3.0~2/pkg.tgz') as tf:
        names = tf.getnames()
        assert 'src/my-file.cpp' in names


def test_repo_import4(dds: DDSWrapper, empty_repo: Path) -> None:
    dds.run(['repo', 'import', empty_repo, PROJECT_ROOT / 'data/simple4.crs'])
    with tarfile.open(empty_repo / 'pkg/test-pkg/1.3.0~3/pkg.tgz') as tf:
        names = tf.getnames()
        assert 'src/deeper/my-file.cpp' in names


def test_repo_import_invalid_crs(dds: DDSWrapper, empty_repo: Path, tmp_project: Project) -> None:
    tmp_project.write('pkg.json', json.dumps({}))
    with expect_error_marker('repo-import-invalid-crs-json'):
        dds.run(['repo', 'import', empty_repo, tmp_project.root])


def test_repo_import_invalid_json(dds: DDSWrapper, empty_repo: Path, tmp_project: Project) -> None:
    tmp_project.write('pkg.json', 'not-json')
    with expect_error_marker('repo-import-invalid-crs-json-parse-error'):
        dds.run(['repo', 'import', empty_repo, tmp_project.root])


def test_repo_import_invalid_nodir(dds: DDSWrapper, empty_repo: Path, tmp_path: Path) -> None:
    with expect_error_marker('repo-import-noent'):
        dds.run(['repo', 'import', empty_repo, tmp_path])


def test_repo_import_invalid_no_repo(dds: DDSWrapper, tmp_path: Path, tmp_project: Project) -> None:
    tmp_project.write('pkg.json', json.dumps({}))
    with expect_error_marker('repo-import-repo-open-fails'):
        dds.run(['repo', 'import', tmp_path, tmp_project.root])


def test_repo_import_db_too_new(dds: DDSWrapper, tmp_path: Path, tmp_project: Project) -> None:
    conn = sqlite3.connect(str(tmp_path / 'repo.db'))
    conn.executescript(r'''
        CREATE TABLE dds_crs_repo_meta (version);
        INSERT INTO dds_crs_repo_meta (version) VALUES (3);
    ''')
    with expect_error_marker('repo-import-db-too-new'):
        dds.run(['repo', 'import', tmp_path, tmp_project.root])


def test_repo_import_db_invalid(dds: DDSWrapper, tmp_path: Path, tmp_project: Project) -> None:
    conn = sqlite3.connect(str(tmp_path / 'repo.db'))
    conn.executescript(r'''
        CREATE TABLE dds_crs_repo_meta (version);
        INSERT INTO dds_crs_repo_meta (version) VALUES ('eggs');
    ''')
    with expect_error_marker('repo-import-db-invalid'):
        dds.run(['repo', 'import', tmp_path, tmp_project.root])


def test_repo_import_db_invalid2(dds: DDSWrapper, tmp_path: Path, tmp_project: Project) -> None:
    tmp_path.joinpath('repo.db').write_bytes(b'not-a-sqlite3-database')
    with expect_error_marker('repo-import-db-invalid'):
        dds.run(['repo', 'import', tmp_path, tmp_project.root])


def test_repo_import_db_invalid3(dds: DDSWrapper, tmp_path: Path, tmp_project: Project) -> None:
    conn = sqlite3.connect(str(tmp_path / 'repo.db'))
    conn.executescript(r'''
        CREATE TABLE dds_crs_repo_meta (version);
        INSERT INTO dds_crs_repo_meta (version) VALUES (1);
    ''')
    with expect_error_marker('repo-import-db-error'):
        dds.run(['repo', 'import', tmp_path, PROJECT_ROOT / 'data/simple.crs'])


def test_repo_double_import(dds: DDSWrapper, empty_repo: Path) -> None:
    crs_dir = PROJECT_ROOT / 'data/simple.crs'
    dds.run(['repo', 'import', empty_repo, crs_dir])
    with expect_error_marker('repo-import-pkg-already-exists'):
        dds.run(['repo', 'import', empty_repo, crs_dir])


def test_repo_double_import_ignore(dds: DDSWrapper, empty_repo: Path) -> None:
    crs_dir = PROJECT_ROOT / 'data/simple.crs'
    dds.run(['repo', 'import', empty_repo, crs_dir])
    before_time = empty_repo.joinpath('pkg/test-pkg/1.2.43~1/pkg.tgz').stat().st_mtime
    dds.run(['repo', 'import', '--if-exists=ignore', empty_repo, crs_dir])
    after_time = empty_repo.joinpath('pkg/test-pkg/1.2.43~1/pkg.tgz').stat().st_mtime
    assert before_time == after_time


def test_repo_double_import_replace(dds: DDSWrapper, empty_repo: Path) -> None:
    crs_dir = PROJECT_ROOT / 'data/simple.crs'
    dds.run(['repo', 'import', empty_repo, crs_dir])
    before_time = empty_repo.joinpath('pkg/test-pkg/1.2.43~1/pkg.tgz').stat().st_mtime
    dds.run(['repo', 'import', '--if-exists=replace', empty_repo, crs_dir])
    after_time = empty_repo.joinpath('pkg/test-pkg/1.2.43~1/pkg.tgz').stat().st_mtime
    assert before_time < after_time


def test_import_multiple_versions(dds: DDSWrapper, empty_repo: Path) -> None:
    for name in ('simple.crs', 'simple2.crs', 'simple3.crs'):
        dds.run(['repo', 'import', empty_repo, PROJECT_ROOT / 'data' / name])
    assert empty_repo.joinpath('pkg/test-pkg/1.2.43~1/pkg.tgz').is_file()
    assert empty_repo.joinpath('pkg/test-pkg/1.3.0~1/pkg.tgz').is_file()
    assert empty_repo.joinpath('pkg/test-pkg/1.3.0~2/pkg.tgz').is_file()

    for name in ('simple.crs', 'simple2.crs', 'simple3.crs'):
        dds.run(['repo', 'import', empty_repo, PROJECT_ROOT / 'data' / name, '--if-exists=ignore'])

    for name in ('simple.crs', 'simple2.crs', 'simple3.crs'):
        dds.run(['repo', 'import', empty_repo, PROJECT_ROOT / 'data' / name, '--if-exists=replace'])
