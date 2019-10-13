#!/usr/bin/env python3

import argparse
import os
from pathlib import Path
import multiprocessing
import itertools
from concurrent.futures import ThreadPoolExecutor
from typing import Sequence, Iterable, Dict, Tuple, List, NamedTuple
import subprocess
import time
import sys

HERE_DIR = Path(__file__).parent.absolute()

INCLUDE_DIRS = [
    'external/taywee-args/include',
    'external/spdlog/include',
    'external/wil/include',
]


class BuildOptions(NamedTuple):
    cxx: Path
    jobs: int
    static: bool
    debug: bool

    @property
    def is_msvc(self) -> bool:
        return is_msvc(self.cxx)

    @property
    def obj_suffix(self) -> str:
        return '.obj' if self.is_msvc else '.o'


def is_msvc(cxx: Path) -> bool:
    return (not 'clang' in cxx.name) and 'cl' in cxx.name


def _create_compile_command(opts: BuildOptions, cpp_file: Path,
                            obj_file: Path) -> List[str]:
    if not opts.is_msvc:
        cmd = [
            str(opts.cxx),
            f'-I{HERE_DIR / "src"}',
            '-std=c++17',
            '-Wall',
            '-Wextra',
            '-Werror',
            '-Wshadow',
            '-Wconversion',
            '-fdiagnostics-color',
            '-pthread',
            '-c',
            str(cpp_file),
            f'-o{obj_file}',
        ]
        if opts.static:
            cmd.append('-static')
        if opts.debug:
            cmd.extend(('-g', '-O0'))
        else:
            cmd.append('-O2')
        cmd.extend(
            itertools.chain.from_iterable(('-isystem', str(HERE_DIR / subdir))
                                          for subdir in INCLUDE_DIRS))
        return cmd
    else:
        cmd = [
            str(opts.cxx),
            '/W4',
            '/WX',
            '/nologo',
            '/EHsc',
            '/std:c++latest',
            f'/I{HERE_DIR / "src"}',
            str(cpp_file),
            '/c',
            f'/Fo{obj_file}',
        ]
        if opts.debug:
            cmd.extend(('/Od', '/DEBUG', '/Z7'))
        else:
            cmd.append('/O2')
        if opts.static:
            cmd.append('/MT')
        else:
            cmd.append('/MD')
        cmd.extend(f'/I{HERE_DIR / subdir}' for subdir in INCLUDE_DIRS)
        return cmd


def _compile_src(opts: BuildOptions, cpp_file: Path) -> Tuple[Path, Path]:
    build_dir = HERE_DIR / '_build'
    src_dir = HERE_DIR / 'src'
    relpath = cpp_file.relative_to(src_dir)
    obj_path = build_dir / relpath.with_name(relpath.name + opts.obj_suffix)
    obj_path.parent.mkdir(exist_ok=True, parents=True)
    cmd = _create_compile_command(opts, cpp_file, obj_path)
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


def compile_sources(opts: BuildOptions,
                    sources: Iterable[Path]) -> Dict[Path, Path]:
    pool = ThreadPoolExecutor(opts.jobs)
    return {
        src: obj
        for src, obj in pool.map(lambda s: _compile_src(opts, s), sources)
    }


def _create_archive_command(opts: BuildOptions,
                            objects: Iterable[Path]) -> Tuple[Path, List[str]]:
    if opts.is_msvc:
        lib_file = HERE_DIR / '_build/libddslim.lib'
        cmd = ['lib', '/nologo', f'/OUT:{lib_file}', *map(str, objects)]
        return lib_file, cmd
    else:
        lib_file = HERE_DIR / '_build/libddslim.a'
        cmd = ['ar', 'rsc', str(lib_file), *map(str, objects)]
        return lib_file, cmd


def make_library(opts: BuildOptions, objects: Iterable[Path]) -> Path:
    lib_file, cmd = _create_archive_command(opts, objects)
    if lib_file.exists():
        lib_file.unlink()
    print(f'Creating static library {lib_file}')
    subprocess.check_call(cmd)
    return lib_file


def _create_exe_link_command(opts: BuildOptions, obj: Path, lib: Path,
                             out: Path) -> List[str]:
    if not opts.is_msvc:
        cmd = [
            str(opts.cxx),
            '-pthread',
            str(obj),
            str(lib),
            '-lstdc++fs',
            f'-o{out}',
        ]
        if opts.static:
            cmd.extend((
                '-static',
                # See: https://stackoverflow.com/questions/35116327/when-g-static-link-pthread-cause-segmentation-fault-why
                '-Wl,--whole-archive',
                '-lpthread',
                '-Wl,--no-whole-archive',
            ))
        return cmd
    else:
        cmd = [
            str(opts.cxx),
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
        if opts.debug:
            cmd.append('/DEBUG')
        if opts.static:
            cmd.append('/MT')
        else:
            cmd.append('/MD')
        return cmd


def link_exe(opts: BuildOptions, obj: Path, lib: Path, *,
             out: Path = None) -> Path:
    if out is None:
        basename = obj.stem
        out = HERE_DIR / '_build/test' / (basename + '.exe')
        out.parent.mkdir(exist_ok=True, parents=True)

    print(f'Linking executable {out}')
    cmd = _create_exe_link_command(opts, obj, lib, out)
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
    parser.add_argument(
        '--jobs',
        '-j',
        type=int,
        help='Set number of parallel build threads',
        default=multiprocessing.cpu_count() + 2)
    parser.add_argument(
        '--debug',
        action='store_true',
        help='Build with debug information and disable optimizations')
    parser.add_argument(
        '--static', action='store_true', help='Build a static executable')
    args = parser.parse_args(argv)

    all_sources = set(HERE_DIR.glob('src/**/*.cpp'))
    test_sources = set(HERE_DIR.glob('src/**/*.test.cpp'))
    main_sources = set(HERE_DIR.glob('src/**/*.main.cpp'))

    lib_sources = (all_sources - test_sources) - main_sources

    build_opts = BuildOptions(
        cxx=Path(args.cxx),
        jobs=args.jobs,
        static=args.static,
        debug=args.debug)

    objects = compile_sources(build_opts, all_sources)

    lib = make_library(build_opts, (objects[p] for p in lib_sources))

    test_objs = (objects[p] for p in test_sources)
    pool = ThreadPoolExecutor(build_opts.jobs)
    test_exes = list(
        pool.map(lambda o: link_exe(build_opts, o, lib), test_objs))

    main_exe = link_exe(
        build_opts,
        objects[next(iter(main_sources))],
        lib,
        out=HERE_DIR / '_build/ddslim')

    if args.test:
        list(pool.map(run_test, test_exes))

    print(f'Main executable generated at {main_exe}')
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
