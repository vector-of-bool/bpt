import subprocess

from dds_ci import paths
from tests import DDS, DDSFixtureParams, dds_fixture_conf, dds_fixture_conf_1
from tests.http import RepoFixture

dds_conf = dds_fixture_conf(
    DDSFixtureParams(ident='git-remote', subdir='git-remote'),
    DDSFixtureParams(ident='no-deps', subdir='no-deps'),
)


@dds_conf
def test_deps_build(dds: DDS, http_repo: RepoFixture) -> None:
    http_repo.import_json_file(dds.source_root / 'catalog.json')
    dds.repo_add(http_repo.url)
    assert not dds.repo_dir.exists()
    dds.build()
    assert dds.repo_dir.exists(), '`Building` did not generate a repo directory'


@dds_fixture_conf_1('use-remote')
def test_use_nlohmann_json_remote(dds: DDS, http_repo: RepoFixture) -> None:
    http_repo.import_json_file(dds.source_root / 'catalog.json')
    dds.repo_add(http_repo.url)
    dds.build(apps=True)

    app_exe = dds.build_dir / f'app{paths.EXE_SUFFIX}'
    assert app_exe.is_file()
    subprocess.check_call([str(app_exe)])
