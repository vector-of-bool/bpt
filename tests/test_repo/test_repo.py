from pathlib import Path
from dds_ci.testing.fixtures import tmp_project

import tarfile
import pytest
import json
import sqlite3

from dds_ci.dds import DDSWrapper
from dds_ci.paths import PROJECT_ROOT
from dds_ci.testing.http import HTTPServerFactory
from dds_ci.testing import Project
from dds_ci.testing.error import expect_error_marker
from dds_ci.testing.repo import CRSRepo, CRSRepoServer


def test_repo_init(tmp_crs_repo: CRSRepo) -> None:
    assert tmp_crs_repo.path.joinpath('repo.db').is_file()
    assert tmp_crs_repo.path.joinpath('repo.db.gz').is_file()


def test_repo_init_already(dds: DDSWrapper, tmp_crs_repo: CRSRepo) -> None:
    with expect_error_marker('repo-init-already-init'):
        dds.run(['repo', 'init', tmp_crs_repo.path, '--name=testing'])


def test_repo_init_ignore(dds: DDSWrapper, tmp_crs_repo: CRSRepo) -> None:
    before_time = tmp_crs_repo.path.joinpath('repo.db').stat().st_mtime
    dds.run(['repo', 'init', tmp_crs_repo.path, '--name=testing', '--if-exists=ignore'])
    after_time = tmp_crs_repo.path.joinpath('repo.db').stat().st_mtime
    assert before_time == after_time


def test_repo_init_replace(dds: DDSWrapper, tmp_crs_repo: CRSRepo) -> None:
    before_time = tmp_crs_repo.path.joinpath('repo.db').stat().st_mtime
    dds.run(['repo', 'init', tmp_crs_repo.path, '--name=testing', '--if-exists=replace'])
    after_time = tmp_crs_repo.path.joinpath('repo.db').stat().st_mtime
    assert before_time < after_time


def test_repo_import(dds: DDSWrapper, tmp_crs_repo: CRSRepo, tmp_project: Project) -> None:
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
    tmp_crs_repo.import_dir(tmp_project.root)


def test_repo_import1(dds: DDSWrapper, tmp_crs_repo: CRSRepo) -> None:
    tmp_crs_repo.import_dir(PROJECT_ROOT / 'data/simple.crs')
    with tarfile.open(tmp_crs_repo.path / 'pkg/test-pkg/1.2.43~1/pkg.tgz') as tf:
        names = tf.getnames()
        assert 'src/my-file.cpp' in names
        assert 'include/my-header.hpp' in names


def test_repo_import2(dds: DDSWrapper, tmp_crs_repo: CRSRepo) -> None:
    tmp_crs_repo.import_dir(PROJECT_ROOT / 'data/simple2.crs')
    with tarfile.open(tmp_crs_repo.path / 'pkg/test-pkg/1.3.0~1/pkg.tgz') as tf:
        names = tf.getnames()
        assert 'include/my-header.hpp' in names


def test_repo_import3(dds: DDSWrapper, tmp_crs_repo: CRSRepo) -> None:
    tmp_crs_repo.import_dir(PROJECT_ROOT / 'data/simple3.crs')
    with tarfile.open(tmp_crs_repo.path / 'pkg/test-pkg/1.3.0~2/pkg.tgz') as tf:
        names = tf.getnames()
        assert 'src/my-file.cpp' in names


def test_repo_import4(dds: DDSWrapper, tmp_crs_repo: CRSRepo) -> None:
    tmp_crs_repo.import_dir(PROJECT_ROOT / 'data/simple4.crs')
    with tarfile.open(tmp_crs_repo.path / 'pkg/test-pkg/1.3.0~3/pkg.tgz') as tf:
        names = tf.getnames()
        assert 'src/deeper/my-file.cpp' in names


def test_repo_import_invalid_crs(dds: DDSWrapper, tmp_crs_repo: CRSRepo, tmp_project: Project) -> None:
    tmp_project.write('pkg.json', json.dumps({}))
    with expect_error_marker('repo-import-invalid-crs-json'):
        tmp_crs_repo.import_dir(tmp_project.root)


def test_repo_import_invalid_json(dds: DDSWrapper, tmp_crs_repo: CRSRepo, tmp_project: Project) -> None:
    tmp_project.write('pkg.json', 'not-json')
    with expect_error_marker('repo-import-invalid-crs-json-parse-error'):
        tmp_crs_repo.import_dir(tmp_project.root)


def test_repo_import_invalid_nodir(dds: DDSWrapper, tmp_crs_repo: CRSRepo, tmp_path: Path) -> None:
    with expect_error_marker('repo-import-noent'):
        tmp_crs_repo.import_dir(tmp_path)


def test_repo_import_invalid_no_repo(dds: DDSWrapper, tmp_path: Path, tmp_project: Project) -> None:
    tmp_project.write('pkg.json', json.dumps({}))
    with expect_error_marker('repo-import-repo-open-fails'):
        dds.run(['repo', 'import', tmp_path, tmp_project.root])


def test_repo_import_db_too_new(dds: DDSWrapper, tmp_path: Path, tmp_project: Project) -> None:
    conn = sqlite3.connect(str(tmp_path / 'repo.db'))
    conn.executescript(r'''
        CREATE TABLE crs_repo_meta (version);
        INSERT INTO crs_repo_meta (version) VALUES (300);
    ''')
    with expect_error_marker('repo-import-db-too-new'):
        dds.run(['repo', 'import', tmp_path, tmp_project.root])


def test_repo_import_db_invalid(dds: DDSWrapper, tmp_path: Path, tmp_project: Project) -> None:
    conn = sqlite3.connect(str(tmp_path / 'repo.db'))
    conn.executescript(r'''
        CREATE TABLE crs_repo_meta (version);
        INSERT INTO crs_repo_meta (version) VALUES ('eggs');
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
        CREATE TABLE crs_repo_meta (version);
        INSERT INTO crs_repo_meta (version) VALUES (1);
    ''')
    with expect_error_marker('repo-import-db-error'):
        dds.run(['repo', 'import', tmp_path, PROJECT_ROOT / 'data/simple.crs'])


def test_repo_double_import(dds: DDSWrapper, tmp_crs_repo: CRSRepo) -> None:
    crs_dir = PROJECT_ROOT / 'data/simple.crs'
    tmp_crs_repo.import_dir(crs_dir)
    with expect_error_marker('repo-import-pkg-already-exists'):
        tmp_crs_repo.import_dir(crs_dir)


def test_repo_double_import_ignore(dds: DDSWrapper, tmp_crs_repo: CRSRepo) -> None:
    crs_dir = PROJECT_ROOT / 'data/simple.crs'
    tmp_crs_repo.import_dir(crs_dir)
    before_time = tmp_crs_repo.path.joinpath('pkg/test-pkg/1.2.43~1/pkg.tgz').stat().st_mtime
    tmp_crs_repo.import_dir(crs_dir, if_exists='ignore')
    after_time = tmp_crs_repo.path.joinpath('pkg/test-pkg/1.2.43~1/pkg.tgz').stat().st_mtime
    assert before_time == after_time


def test_repo_double_import_replace(dds: DDSWrapper, tmp_crs_repo: CRSRepo) -> None:
    crs_dir = PROJECT_ROOT / 'data/simple.crs'
    tmp_crs_repo.import_dir(crs_dir)
    before_time = tmp_crs_repo.path.joinpath('pkg/test-pkg/1.2.43~1/pkg.tgz').stat().st_mtime
    tmp_crs_repo.import_dir(crs_dir, if_exists='replace')
    after_time = tmp_crs_repo.path.joinpath('pkg/test-pkg/1.2.43~1/pkg.tgz').stat().st_mtime
    assert before_time < after_time


def test_import_multiple_versions(tmp_crs_repo: CRSRepo) -> None:
    for name in ('simple.crs', 'simple2.crs', 'simple3.crs'):
        tmp_crs_repo.import_dir(PROJECT_ROOT / 'data' / name)
    assert tmp_crs_repo.path.joinpath('pkg/test-pkg/1.2.43~1/pkg.tgz').is_file()
    assert tmp_crs_repo.path.joinpath('pkg/test-pkg/1.3.0~1/pkg.tgz').is_file()
    assert tmp_crs_repo.path.joinpath('pkg/test-pkg/1.3.0~2/pkg.tgz').is_file()

    for name in ('simple.crs', 'simple2.crs', 'simple3.crs'):
        tmp_crs_repo.import_dir(PROJECT_ROOT / 'data' / name, if_exists='ignore')

    for name in ('simple.crs', 'simple2.crs', 'simple3.crs'):
        tmp_crs_repo.import_dir(PROJECT_ROOT / 'data' / name, if_exists='replace')


def test_pkg_prefetch(dds: DDSWrapper, http_crs_repo: CRSRepoServer) -> None:
    http_crs_repo.repo.import_dir(PROJECT_ROOT / 'data/simple.crs')
    dds.run(['pkg', 'prefetch', '-r', http_crs_repo.server.base_url])


def test_pkg_prefetch_404(dds: DDSWrapper, tmp_path: Path, http_server_factory: HTTPServerFactory) -> None:
    srv = http_server_factory(tmp_path)
    with expect_error_marker('repo-sync-http-404'):
        dds.run(['pkg', 'prefetch', '-r', srv.base_url])


def test_pkg_prefetch_invalid(dds: DDSWrapper, tmp_path: Path, http_server_factory: HTTPServerFactory) -> None:
    tmp_path.joinpath('repo.db.gz').write_text('lolhi')
    srv = http_server_factory(tmp_path)
    with expect_error_marker('repo-sync-invalid-db-gz'):
        dds.run(['-ltrace', 'pkg', 'prefetch', '-r', srv.base_url])
