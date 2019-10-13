#!/usr/bin/env python3

import argparse
import os
from pathlib import Path
import multiprocessing
import itertools
from concurrent.futures import ThreadPoolExecutor
from typing import Sequence, Iterable, Dict, Tuple,List
import subprocess
import time
import sys

HERE_DIR = Path(__file__).parent.absolute()

INCLUDE_DIRS = [
    'external/taywee-args/include',
    'external/spdlog/include',
    'external/wil/include',
]


def is_msvc(cxx: Path) -> bool:
    return (not 'clang' in cxx.name) and 'cl' in cxx.name


def _create_compile_command(cxx: Path, cpp_file: Path, obj_file: Path) -> List[str]:
    if not is_msvc(cxx):
        cmd = [
            str(cxx),
            f'-I{HERE_DIR / "src"}',
            '-std=c++17',
            '-static',
            '-Wall',
            '-Wextra',
            '-Werror',
            '-Wshadow',
            '-Wconversion',
            '-fdiagnostics-color',
            '-pthread',
            '-g',
            '-c',
            '-O0',
            str(cpp_file),
            f'-o{obj_file}',
        ]
        cmd.extend(
            itertools.chain.from_iterable(
                ('-isystem', str(HERE_DIR / subdir)) for subdir in INCLUDE_DIRS))
        return cmd
    else:
        cmd = [
            str(cxx),
            # '/O2',
            '/Od',
            '/Z7',
            '/DEBUG',
            '/W4',
            '/WX',
            '/MT',
            '/nologo',
            # '/wd2220',
            '/EHsc',
            '/std:c++latest',
            f'/I{HERE_DIR / "src"}',
            str(cpp_file),
            '/c',
            f'/Fo{obj_file}',
        ]
        cmd.extend(
                f'/I{HERE_DIR / subdir}' for subdir in INCLUDE_DIRS)
        return cmd


def _compile_src(cxx: Path, cpp_file: Path) -> Tuple[Path, Path]:
    build_dir = HERE_DIR / '_build'
    src_dir = HERE_DIR / 'src'
    relpath = cpp_file.relative_to(src_dir)
    obj_path = build_dir / relpath.with_name(relpath.name + ('.obj' if is_msvc(cxx) else '.o'))
    obj_path.parent.mkdir(exist_ok=True, parents=True)
    cmd = _create_compile_command(cxx, cpp_file, obj_path)
    msg = f'Compile C++ file: {cpp_file}'
    print(msg)
    start = time.time()
    res = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if res.returncode != 0:
        raise RuntimeError(
            f'Compile command ({cmd}) failed for {cpp_file}:\n{res.stdout.decode()}'
        )
    if res.stdout:
        print(res.stdout.decode())
    end = time.time()
    print(f'{msg} - Done: {end - start:.2}s')
    return cpp_file, obj_path


def compile_sources(cxx: Path, sources: Iterable[Path], *, jobs: int) -> Dict[Path, Path]:
    pool = ThreadPoolExecutor(jobs)
    return {
        src: obj
        for src, obj in pool.map(lambda s: _compile_src(cxx, s), sources)
    }


def _create_archive_command(objects: Iterable[Path]) -> List[str]:
    if os.name == 'nt':
        lib_file = HERE_DIR / '_build/libddslim.lib'
        cmd = ['lib', '/nologo', f'/OUT:{lib_file}', *map(str, objects)]
        return lib_file, cmd
    else:
        lib_file = HERE_DIR / '_build/libddslim.a'
        cmd = ['ar', 'rsc', str(lib_file), *objects]
        return lib_file, cmd


def make_library(objects: Iterable[Path]) -> Path:
    lib_file, cmd = _create_archive_command(objects)
    if lib_file.exists():
        lib_file.unlink()
    print(f'Creating static library {lib_file}')
    subprocess.check_call(cmd)
    return lib_file


def _create_exe_link_command(cxx: Path, obj: Path, lib: Path, out: Path) -> List[str]:
    if not is_msvc(cxx):
        return [
            str(cxx),
            '-static',
            '-pthread',
            # See: https://stackoverflow.com/questions/35116327/when-g-static-link-pthread-cause-segmentation-fault-why
            '-Wl,--whole-archive',
            '-lpthread',
            '-Wl,--no-whole-archive',
            str(obj),
            str(lib),
            '-lstdc++fs',
            f'-o{out}',
        ]
    else:
        return [
            str(cxx),
            '/nologo',
            '/W4',
            '/WX',
            '/MT',
            '/Z7',
            '/DEBUG',
            f'/Fe{out}',
            str(lib),
            str(obj),
        ]


def link_exe(cxx: Path, obj: Path, lib: Path, *, out: Path = None) -> Path:
    if out is None:
        basename = obj.stem
        out = HERE_DIR / '_build/test' / (basename + '.exe')
        out.parent.mkdir(exist_ok=True, parents=True)

    print(f'Linking executable {out}')
    cmd = _create_exe_link_command(cxx, obj, lib, out)
    subprocess.check_call(cmd)
    return out


def run_test(exe: Path) -> None:
    print(f'Running test: {exe}')
    subprocess.check_call([str(exe)])


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--test', action='store_true', help='Build and run tests')
    parser.add_argument(
        '--cxx', help='Path/name of the C++ compiler to use.', required=True)
    parser.add_argument('--jobs', '-j', type=int, help='Set number of parallel build threads', default=multiprocessing.cpu_count() + 2)
    args = parser.parse_args(argv)

    all_sources = set(HERE_DIR.glob('src/**/*.cpp'))
    test_sources = set(HERE_DIR.glob('src/**/*.test.cpp'))
    main_sources = set(HERE_DIR.glob('src/**/*.main.cpp'))

    lib_sources = (all_sources - test_sources) - main_sources

    objects = compile_sources(Path(args.cxx), all_sources, jobs=args.jobs)

    lib = make_library(objects[p] for p in lib_sources)

    test_objs = (objects[p] for p in test_sources)
    pool = ThreadPoolExecutor(args.jobs)
    test_exes = list(
        pool.map(lambda o: link_exe(Path(args.cxx), o, lib), test_objs))

    main_exe = link_exe(
        Path(args.cxx),
        objects[next(iter(main_sources))],
        lib,
        out=HERE_DIR / '_build/ddslim')

    if args.test:
        list(pool.map(run_test, test_exes))

    print(f'Main executable generated at {main_exe}')
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
