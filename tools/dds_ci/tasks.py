from __future__ import annotations

import asyncio
import json
import os
import re
import sys
from pathlib import Path

from dagon import fs, option, proc, task, ui

from . import paths
from .bootstrap import BootstrapMode, get_bootstrap_exe, pin_exe
from .dds import DDSWrapper
from .toolchain import (fixup_toolchain, get_default_audit_toolchain, get_default_toolchain)
from .util import Pathish

FRAC_RE = re.compile(r'(\d+)/(\d+)')

bootstrap_mode = option.add('bootstrap-mode',
                            BootstrapMode,
                            default=BootstrapMode.Lazy,
                            doc='How should we obtain the prior version of dds?')

main_tc = option.add('main-toolchain',
                     Path,
                     default=get_default_toolchain(),
                     doc='The toolchain to use when building the release executable')
test_tc = option.add('test-toolchain',
                     Path,
                     default=get_default_audit_toolchain(),
                     doc='The toolchain to use when building for testing')

jobs = option.add('jobs', int, default=6, doc='Number of parallel jobs to run during build and test')

main_build_dir = option.add('build.main.dir',
                            Path,
                            default=paths.BUILD_DIR,
                            doc='Directory in which to store the main build results')
test_build_dir = option.add('build.test.dir',
                            Path,
                            default=paths.BUILD_DIR / 'for-test',
                            doc='Directory in which to store the test build results')

PRIOR_CACHE_ARGS = [
    f'--pkg-db-path={paths.PREBUILT_DIR/"ci-catalog.db"}',
    f'--pkg-cache-dir={paths.PREBUILT_DIR  / "ci-repo"}',
]

NEW_CACHE_ARGS = [
    f'--crs-cache-dir={paths.PREBUILT_DIR  / "_crs"}',
]


def _progress(record: proc.ProcessOutputItem) -> None:
    line = record.out.decode()
    ui.status(line)
    mat = FRAC_RE.search(line)
    if not mat:
        return
    num, denom = mat.groups()
    ui.progress(int(num) / int(denom))


async def _build_with_tc(dds: DDSWrapper, into: Pathish, tc: Path, *, args: proc.CommandLine = ()) -> DDSWrapper:
    into = Path(into)
    with pin_exe(dds.path) as pinned:
        with fixup_toolchain(tc) as tc1:
            ui.print(f'Generating a build of dds using the [{tc1}] toolchain.')
            ui.print(f'  This build result will be written to [{into}]')
            await proc.run(
                [
                    pinned,
                    'build',
                    '-o',
                    into,
                    '-j',
                    jobs.get(),
                    f'--toolchain={tc1}',
                    '--tweaks-dir',
                    paths.TWEAKS_DIR,
                    args,
                ],
                on_output=_progress,
                print_output_on_finish='always',
            )
    return DDSWrapper(into / 'dds')


@task.define()
async def clean():
    ui.status('Removing prior build')
    await fs.remove(
        [
            paths.BUILD_DIR,
            paths.PREBUILT_DIR,
            *paths.PROJECT_ROOT.rglob('__pycache__'),
            *paths.PROJECT_ROOT.rglob('*.stamp'),
        ],
        absent_ok=True,
        recurse=True,
    )


@task.define(order_only_depends=[clean])
async def bootstrap() -> DDSWrapper:
    ui.status('Obtaining prior DDS version')
    d = get_bootstrap_exe(bootstrap_mode.get())
    ui.status('Loading legacy repository information')
    await proc.run(
        [
            d.path,
            'pkg',
            'repo',
            'add',
            'https://repo-1.dds.pizza',
            PRIOR_CACHE_ARGS,
        ],
        on_output='status',
    )
    return d


@task.define(depends=[bootstrap])
async def build__init_ci_repo() -> None:
    "Runs a compile-file just to get the CI repository available"
    dds = await task.result_of(bootstrap)
    await proc.run(
        [
            dds.path,
            PRIOR_CACHE_ARGS,
            'compile-file',
            '-t',
            main_tc.get(),
        ],
        on_output='status',
    )


@task.define(depends=[bootstrap, build__init_ci_repo])
async def build__main() -> DDSWrapper:
    dds = await task.result_of(bootstrap)
    return await _build_with_tc(dds, main_build_dir.get(), main_tc.get(), args=[PRIOR_CACHE_ARGS, '--no-tests'])


@task.define(depends=[bootstrap, build__init_ci_repo])
async def build__test() -> DDSWrapper:
    dds = await task.result_of(bootstrap)
    return await _build_with_tc(dds, test_build_dir.get(), test_tc.get(), args=PRIOR_CACHE_ARGS)


which_file = option.add('compile-file', Path, doc='Which file will be compiled by the "compile-file" task')


@task.define(depends=[bootstrap, build__init_ci_repo])
async def compile_file() -> None:
    dds = await task.result_of(bootstrap)
    with fixup_toolchain(test_tc.get()) as tc:
        await proc.run([
            dds.path,
            'compile-file',
            which_file.get(),
            f'--toolchain={tc}',
            f'--tweaks-dir={paths.TWEAKS_DIR}',
            f'--out={test_build_dir.get()}',
            f'--project={paths.PROJECT_ROOT}',
            PRIOR_CACHE_ARGS,
        ])


@task.define(depends=[build__test])
async def test() -> None:
    basetemp = Path('/tmp/dds-ci')
    basetemp.mkdir(exist_ok=True, parents=True)
    dds = await task.result_of(build__test)
    await proc.run(
        [
            sys.executable,
            '-m',
            'pytest',
            '-v',
            '--durations=10',
            f'-n{jobs.get()}',
            f'--basetemp={basetemp}',
            f'--dds-exe={dds.path}',
            f'--junit-xml={paths.BUILD_DIR}/pytest-junit.xml',
            paths.PROJECT_ROOT / 'tests',
        ],
        on_output='status',
        print_output_on_finish='always',
    )


@task.define(order_only_depends=[clean])
async def __get_catch2() -> Path:
    tag = 'v2.13.7'
    into = paths.BUILD_DIR / f'_catch2-{tag}'
    ui.status('Cloning Catch2')
    if not into.is_dir():
        await proc.run([
            'git', 'clone', 'https://github.com/catchorg/Catch2.git', f'--branch={tag}', '--depth=1',
            into.with_suffix('.tmp')
        ])
        into.with_suffix('.tmp').rename(into)
    proj_dir = into / '_proj'
    proj_dir.mkdir(exist_ok=True)
    proj_dir.joinpath('pkg.yaml').write_text(
        json.dumps({
            'name':
            'catch2',
            'version':
            tag[1:],
            'libs': [{
                'path': '.',
                'name': 'catch2',
            }, {
                'path': 'mainlib',
                'name': 'main',
                'using': ['catch2'],
            }],
        }))
    inc_dir = proj_dir / 'include'
    if not inc_dir.is_dir():
        await fs.copy_tree(into / 'single_include/', inc_dir, if_exists='merge', if_file_exists='keep')
    main_src = proj_dir / 'mainlib/src'
    main_src.mkdir(exist_ok=True, parents=True)
    main_src.joinpath('catch2_main.cpp').write_text('''
        #define CATCH_CONFIG_MAIN
        #include <catch2/catch.hpp>
    ''')
    return proj_dir


@task.define(depends=[build__test, __get_catch2])
async def self_build_repo() -> Path:
    dds = await task.result_of(build__test)
    repo_dir = paths.PREBUILT_DIR / '_crs-repo'
    await proc.run([dds.path, NEW_CACHE_ARGS, 'repo', 'init', repo_dir, '--name=.tmp.', '--if-exists=ignore'])
    packages = [
        "spdlog@1.7.0",
        "ms-wil@2020.3.16",
        "range-v3@0.11.0",
        "nlohmann-json@3.9.1",
        "neo-sqlite3@0.7.1",
        "neo-fun^0.11.1",
        "neo-buffer^0.5.2",
        "neo-compress^0.3.1",
        "neo-io@0.2.3",
        "neo-url^0.2.5",
        "semver@0.2.2",
        "pubgrub^0.3.1",
        "vob-json5@0.1.6",
        "vob-semester@0.3.1",
        "ctre@2.8.1",
        "fmt^6.2.1",
        "neo-http^0.2.0",
        "boost.leaf^1.78.0",
        "magic_enum+0.7.3",
        "sqlite3^3.35.2",
        "yaml-cpp@0.7.0",
        "zlib@1.2.9",
    ]
    for pkg in packages:
        await _repo_import_pkg(dds, pkg, repo_dir)
    catch2 = await task.result_of(__get_catch2)
    await _repo_build_and_import_dir(dds, catch2, repo_dir)
    ui.status('Validating repository')
    await proc.run([dds.path, 'repo', 'validate', repo_dir])
    return repo_dir


SELF_REPO_DEPS = {
    'neo-buffer': ['neo-fun@0.11.1 using neo-fun'],
    'neo-sqlite3': ['neo-fun@0.11.1 using neo-fun', 'sqlite3@3.35.2 using sqlite3'],
    'neo-url': ['neo-fun@0.11.1 using neo-fun'],
    'neo-compress': ['neo-buffer@0.5.2 using neo-buffer', 'zlib@1.2.9 using zlib'],
    'neo-io': ['neo-buffer@0.5.2 using neo-buffer'],
    'neo-http': ['neo-io@0.2.3 using neo-io'],
}


async def _repo_import_pkg(dds: DDSWrapper, pkg: str, repo_dir: Path) -> None:
    mat = re.match(r'(.+)[@=+~^](.+)', pkg)
    assert mat, pkg
    name, ver = mat.groups()
    pid = f'{name}@{ver}'
    pulled = paths.PREBUILT_DIR / 'ci-repo' / pid
    assert pulled.is_dir(), pid
    pulled.joinpath('pkg.yaml').write_text(
        json.dumps({
            'name': name,
            'version': ver,
            'dependencies': SELF_REPO_DEPS.get(name, []),
        }))
    ui.status(f'Importing package: {pkg}')
    await _repo_build_and_import_dir(dds, pulled, repo_dir)


async def _repo_build_and_import_dir(dds: DDSWrapper, proj: Path, repo_dir: Path) -> None:
    # await proc.run([dds.path, NEW_CACHE_ARGS, 'build', '-p', repo_dir, '-t', test_tc.get(), '-o', proj / '_build'])
    await proc.run([dds.path, NEW_CACHE_ARGS, 'repo', 'import', repo_dir, proj, '--if-exists=replace'])


@task.define(depends=[build__test, self_build_repo])
async def self_build() -> DDSWrapper:
    dds = await task.result_of(build__test)
    self_repo = await task.result_of(self_build_repo)
    return await _build_with_tc(
        dds,
        test_build_dir.get() / 'self',
        main_tc.get(),
        args=[f'--use-repo={self_repo.as_uri()}', NEW_CACHE_ARGS, '--no-default-repo'],
    )


@task.define(order_only_depends=[clean])
async def docs() -> None:
    ui.status('Building documentation with Sphinx')
    await proc.run(
        [
            sys.executable,
            '-m',
            'sphinx',
            paths.PROJECT_ROOT / 'docs',
            paths.BUILD_DIR / 'docs',
            '-d',
            paths.BUILD_DIR / 'doctrees',
            '-anj8',
        ],
        print_output_on_finish='always',
    )


def _find_clang_format() -> Path:
    for cf_cand in ('clang-format-10', 'clang-format-9', 'clang-format-8', 'clang-format'):
        cf = paths.find_exe(cf_cand)
        if cf:
            return cf
    raise RuntimeError('No clang-format executable found')


async def _run_clang_format(args: proc.CommandLine):
    cf = _find_clang_format()
    await proc.run(
        [cf, args, paths.PROJECT_ROOT.glob('src/**/*.[hc]pp')],
        on_output='status',
    )


async def _run_yapf(args: proc.CommandLine):
    tools_py = paths.TOOLS_DIR.rglob('*.py')
    tests_py = paths.TESTS_DIR.rglob('*.py')
    await proc.run(
        [
            sys.executable,
            '-um',
            'yapf',
            args,
            tools_py,
            tests_py,
        ],
        on_output='status',
    )


@task.define()
async def format__cpp():
    await _run_clang_format(['-i', '--verbose'])


@task.define()
async def format__py():
    await _run_yapf(['--in-place', '--verbose'])


@task.define(order_only_depends=[format__cpp])
async def format__cpp__check():
    ui.status("Checking C++ formatting...")
    await _run_clang_format(['--Werror', '--dry-run'])


@task.define(order_only_depends=[format__py])
async def format__py__check():
    ui.status('Checking Python formatting...')
    await _run_yapf(['--diff'])


format_ = task.gather('format', [format__cpp, format__py])
format__check = task.gather('format.check', [format__cpp__check, format__py__check])

pyright = proc.cmd_task('pyright', [sys.executable, '-m', 'pyright', paths.TOOLS_DIR])
pylint = proc.cmd_task('pylint', [sys.executable, '-m', 'pylint', paths.TOOLS_DIR])

py = task.gather('py', [format__py__check, pylint, pyright])

ci = task.gather('ci', [test, build__main, py, format__check])

if os.name == 'nt':
    # Workaround: Older Python does not set the event loop such that Windows can
    # use subprocesses asynchronously
    asyncio.set_event_loop(asyncio.ProactorEventLoop())
