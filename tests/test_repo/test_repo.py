from pathlib import Path

import tarfile
import pytest
import json
import sqlite3

from bpt_ci.bpt import BPTWrapper
from bpt_ci.paths import PROJECT_ROOT
from bpt_ci.testing.http import HTTPServerFactory
from bpt_ci.testing import Project
from bpt_ci.testing.error import expect_error_marker
from bpt_ci.testing.repo import CRSRepo, CRSRepoFactory, make_simple_crs


def test_repo_init(tmp_crs_repo: CRSRepo) -> None:
    assert tmp_crs_repo.path.joinpath('repo.db').is_file()
    assert tmp_crs_repo.path.joinpath('repo.db.gz').is_file()


def test_repo_init_already(bpt: BPTWrapper, tmp_crs_repo: CRSRepo) -> None:
    with expect_error_marker('repo-init-already-init'):
        bpt.run(['repo', 'init', tmp_crs_repo.path, '--name=testing'])


def test_repo_init_ignore(bpt: BPTWrapper, tmp_crs_repo: CRSRepo) -> None:
    before_time = tmp_crs_repo.path.joinpath('repo.db').stat().st_mtime
    bpt.run(['repo', 'init', tmp_crs_repo.path, '--name=testing', '--if-exists=ignore'])
    after_time = tmp_crs_repo.path.joinpath('repo.db').stat().st_mtime
    assert before_time == after_time


def test_repo_init_replace(bpt: BPTWrapper, tmp_crs_repo: CRSRepo) -> None:
    before_time = tmp_crs_repo.path.joinpath('repo.db').stat().st_mtime
    bpt.run(['repo', 'init', tmp_crs_repo.path, '--name=testing', '--if-exists=replace'])
    after_time = tmp_crs_repo.path.joinpath('repo.db').stat().st_mtime
    assert before_time < after_time


def test_repo_import(bpt: BPTWrapper, tmp_crs_repo: CRSRepo, tmp_project: Project) -> None:
    tmp_project.write(
        'pkg.json',
        json.dumps({
            'schema-version':
            1,
            'name':
            'meow',
            'version':
            '1.2.3',
            'pkg-version':
            1,
            'libraries': [{
                'name': 'test',
                'path': '.',
                'using': [],
                'test-using': [],
                'dependencies': [],
                'test-dependencies': [],
            }],
        }))
    tmp_crs_repo.import_(tmp_project.root)


def test_repo_import1(bpt: BPTWrapper, tmp_crs_repo: CRSRepo) -> None:
    tmp_crs_repo.import_(PROJECT_ROOT / 'data/simple.crs', validate=False)
    with tarfile.open(tmp_crs_repo.path / 'pkg/test-pkg/1.2.43~1/pkg.tgz') as tf:
        names = tf.getnames()
        assert 'src/my-file.cpp' in names
        assert 'include/my-header.hpp' in names


def test_repo_import2(bpt: BPTWrapper, tmp_crs_repo: CRSRepo) -> None:
    tmp_crs_repo.import_(PROJECT_ROOT / 'data/simple2.crs', validate=False)
    with tarfile.open(tmp_crs_repo.path / 'pkg/test-pkg/1.3.0~1/pkg.tgz') as tf:
        names = tf.getnames()
        assert 'include/my-header.hpp' in names


def test_repo_import3(bpt: BPTWrapper, tmp_crs_repo: CRSRepo) -> None:
    tmp_crs_repo.import_(PROJECT_ROOT / 'data/simple3.crs', validate=False)
    with tarfile.open(tmp_crs_repo.path / 'pkg/test-pkg/1.3.0~2/pkg.tgz') as tf:
        names = tf.getnames()
        assert 'src/my-file.cpp' in names


def test_repo_import4(bpt: BPTWrapper, tmp_crs_repo: CRSRepo) -> None:
    tmp_crs_repo.import_(PROJECT_ROOT / 'data/simple4.crs', validate=False)
    with tarfile.open(tmp_crs_repo.path / 'pkg/test-pkg/1.3.0~3/pkg.tgz') as tf:
        names = tf.getnames()
        assert 'src/deeper/my-file.cpp' in names


def test_repo_import_invalid_crs(bpt: BPTWrapper, tmp_crs_repo: CRSRepo, tmp_project: Project) -> None:
    tmp_project.write('pkg.json', json.dumps({}))
    with expect_error_marker('repo-import-invalid-crs-json'):
        tmp_crs_repo.import_(tmp_project.root)


def test_repo_import_invalid_json(bpt: BPTWrapper, tmp_crs_repo: CRSRepo, tmp_project: Project) -> None:
    tmp_project.write('pkg.json', 'not-json')
    with expect_error_marker('repo-import-invalid-crs-json-parse-error'):
        tmp_crs_repo.import_(tmp_project.root)


def test_repo_import_invalid_nodir(bpt: BPTWrapper, tmp_crs_repo: CRSRepo, tmp_path: Path) -> None:
    with expect_error_marker('repo-import-noent'):
        tmp_crs_repo.import_(tmp_path)


def test_repo_import_invalid_no_repo(bpt: BPTWrapper, tmp_path: Path, tmp_project: Project) -> None:
    tmp_project.write('pkg.json', json.dumps({}))
    with expect_error_marker('repo-repo-open-fails'):
        bpt.run(['repo', 'import', tmp_path, tmp_project.root])


def test_repo_import_db_too_new(bpt: BPTWrapper, tmp_path: Path, tmp_project: Project) -> None:
    conn = sqlite3.connect(str(tmp_path / 'repo.db'))
    conn.executescript(r'''
        CREATE TABLE crs_repo_meta (version);
        INSERT INTO crs_repo_meta (version) VALUES (300);
    ''')
    with expect_error_marker('repo-db-too-new'):
        bpt.run(['repo', 'import', tmp_path, tmp_project.root])


def test_repo_import_db_invalid(bpt: BPTWrapper, tmp_path: Path, tmp_project: Project) -> None:
    conn = sqlite3.connect(str(tmp_path / 'repo.db'))
    conn.executescript(r'''
        CREATE TABLE crs_repo_meta (version);
        INSERT INTO crs_repo_meta (version) VALUES ('eggs');
    ''')
    with expect_error_marker('repo-db-invalid'):
        bpt.run(['repo', 'import', tmp_path, tmp_project.root])


def test_repo_import_db_invalid2(bpt: BPTWrapper, tmp_path: Path, tmp_project: Project) -> None:
    tmp_path.joinpath('repo.db').write_bytes(b'not-a-sqlite3-database')
    with expect_error_marker('repo-db-invalid'):
        bpt.run(['repo', 'import', tmp_path, tmp_project.root])


def test_repo_import_db_invalid3(bpt: BPTWrapper, tmp_path: Path, tmp_project: Project) -> None:
    conn = sqlite3.connect(str(tmp_path / 'repo.db'))
    conn.executescript(r'''
        CREATE TABLE crs_repo_meta (version);
        INSERT INTO crs_repo_meta (version) VALUES (1);
    ''')
    with expect_error_marker('repo-import-db-error'):
        bpt.run(['repo', 'import', tmp_path, PROJECT_ROOT / 'data/simple.crs'])


@pytest.fixture(scope='session')
def simple_repo(crs_repo_factory: CRSRepoFactory) -> CRSRepo:
    repo = crs_repo_factory('simple')
    names = ('simple.crs', 'simple2.crs', 'simple3.crs', 'simple4.crs')
    simples = (PROJECT_ROOT / 'data' / name for name in names)
    repo.import_(simples, validate=False)
    return repo


def test_repo_double_import(bpt: BPTWrapper, simple_repo: CRSRepo) -> None:
    with expect_error_marker('repo-import-pkg-already-exists'):
        simple_repo.import_(PROJECT_ROOT / 'data/simple.crs')


def test_repo_double_import_ignore(bpt: BPTWrapper, simple_repo: CRSRepo) -> None:
    before_time = simple_repo.path.joinpath('pkg/test-pkg/1.2.43~1/pkg.tgz').stat().st_mtime
    simple_repo.import_(PROJECT_ROOT / 'data/simple.crs', if_exists='ignore', validate=False)
    after_time = simple_repo.path.joinpath('pkg/test-pkg/1.2.43~1/pkg.tgz').stat().st_mtime
    assert before_time == after_time


def test_repo_double_import_replace(bpt: BPTWrapper, simple_repo: CRSRepo) -> None:
    before_time = simple_repo.path.joinpath('pkg/test-pkg/1.2.43~1/pkg.tgz').stat().st_mtime
    simple_repo.import_(PROJECT_ROOT / 'data/simple.crs', if_exists='replace', validate=False)
    after_time = simple_repo.path.joinpath('pkg/test-pkg/1.2.43~1/pkg.tgz').stat().st_mtime
    assert before_time < after_time


def test_pkg_prefetch_http_url(bpt: BPTWrapper, simple_repo: CRSRepo, http_server_factory: HTTPServerFactory,
                               tmp_path: Path) -> None:
    srv = http_server_factory(simple_repo.path)
    bpt.crs_cache_dir = tmp_path
    bpt.pkg_prefetch(repos=[srv.base_url], pkgs=['test-pkg@1.2.43'])
    assert tmp_path.joinpath('pkgs/test-pkg@1.2.43~1/pkg.json').is_file()


def test_pkg_prefetch_file_url(bpt: BPTWrapper, tmp_path: Path, simple_repo: CRSRepo) -> None:
    bpt.crs_cache_dir = tmp_path
    bpt.pkg_prefetch(repos=[str(simple_repo.path)], pkgs=['test-pkg@1.2.43'])
    assert tmp_path.joinpath('pkgs/test-pkg@1.2.43~1/pkg.json').is_file()


def test_pkg_prefetch_404(bpt: BPTWrapper, tmp_path: Path, http_server_factory: HTTPServerFactory) -> None:
    srv = http_server_factory(tmp_path)
    bpt.crs_cache_dir = tmp_path
    with expect_error_marker('repo-sync-http-404'):
        bpt.pkg_prefetch(repos=[srv.base_url])


def test_pkg_prefetch_invalid_tgz(bpt: BPTWrapper, tmp_path: Path, http_server_factory: HTTPServerFactory) -> None:
    tmp_path.joinpath('repo.db.gz').write_text('lolhi')
    srv = http_server_factory(tmp_path)
    bpt.crs_cache_dir = tmp_path
    with expect_error_marker('repo-sync-invalid-db-gz'):
        bpt.pkg_prefetch(repos=[srv.base_url])


def test_repo_validate_empty(tmp_crs_repo: CRSRepo) -> None:
    tmp_crs_repo.validate()


def test_repo_validate_simple(tmp_crs_repo: CRSRepo, tmp_path: Path) -> None:
    tmp_path.joinpath('pkg.json').write_text(
        json.dumps({
            'name':
            'foo',
            'version':
            '1.2.3',
            'pkg-version':
            1,
            'schema-version':
            1,
            'libraries': [{
                'path': '.',
                'name': 'foo',
                'using': [],
                'test-using': [],
                'dependencies': [],
                'test-dependencies': [],
            }],
        }))
    tmp_crs_repo.import_(tmp_path)
    tmp_crs_repo.validate()


def test_repo_validate_interdep(tmp_crs_repo: CRSRepo, tmp_path: Path) -> None:
    # yapf: disable
    tmp_path.joinpath('pkg.json').write_text(
        json.dumps({
            'name': 'foo',
            'version': '1.2.3',
            'pkg-version': 1,
            'schema-version': 1,
            'libraries': [{
                'path': '.',
                'name': 'foo',
                'test-using': [],
                'using': ['bar'],
                'dependencies': [],
                'test-dependencies': [],
            }, {
                'path': 'bar',
                'name': 'bar',
                'using': [],
                'test-using': [],
                'dependencies': [],
                'test-dependencies': [],
            }],
        }))
    # yapf: enable
    tmp_crs_repo.import_(tmp_path)
    tmp_crs_repo.validate()


def test_repo_validate_invalid_no_sibling(tmp_crs_repo: CRSRepo, tmp_project: Project) -> None:
    tmp_project.bpt_yaml = {
        'name': 'foo',
        'version': '1.2.3',
        'libraries': [{
            'path': '.',
            'name': 'foo',
            'using': ['bar'],
        }],
    }
    with expect_error_marker('repo-import-invalid-proj-json'):
        tmp_crs_repo.import_(tmp_project.root)


def test_repo_invalid_pkg_version_zero(tmp_crs_repo: CRSRepo, tmp_path: Path) -> None:
    tmp_path.joinpath('pkg.json').write_text(json.dumps(make_simple_crs('foo', '1.2.3', pkg_version=0)))
    with expect_error_marker('repo-import-invalid-pkg-version'):
        tmp_crs_repo.import_(tmp_path)


def test_repo_no_use_invalid_pkg_version(tmp_crs_repo: CRSRepo, tmp_project: Project) -> None:
    '''
    Check that BPT refuses to acknowledge remote packages that have an invalid (<1) pkg revision.

    The 'repo import' utility will refuse to import them, but a hostile server could still
    serve them.
    '''
    # Replace the repo db with our own
    db_path = tmp_crs_repo.path / 'repo.db'
    db_path.unlink()
    db = sqlite3.connect(str(db_path))
    db.executescript(r'''
        CREATE TABLE crs_repo_self(rowid INTEGER PRIMARY KEY, name TEXT NOT NULL);
        INSERT INTO crs_repo_self VALUES(1729, 'test');
        CREATE TABLE crs_repo_packages(
            package_id INTEGER PRIMARY KEY,
            meta_json TEXT NOT NULL
        );
    ''')
    with db:
        db.execute(r'INSERT INTO crs_repo_packages(meta_json) VALUES(?)',
                   [json.dumps(make_simple_crs('bar', '1.2.3', pkg_version=1))])

    tmp_project.bpt_yaml = {
        'name': 'foo',
        'version': '1.2.3',
        'dependencies': ['bar@1.2.3'],
    }
    tmp_project.bpt.run(['pkg', 'solve', '-r', tmp_crs_repo.path, 'bar@1.2.3'])
    # Replace with a bad pkg_version:
    with db:
        db.execute('UPDATE crs_repo_packages SET meta_json=?',
                   [json.dumps(make_simple_crs('bar', '1.2.3', pkg_version=0))])
    with expect_error_marker('no-dependency-solution'):
        tmp_project.bpt.run(['pkg', 'solve', '-r', tmp_crs_repo.path, 'bar@1.2.3'])
