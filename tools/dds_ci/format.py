import argparse
from typing_extensions import Protocol

import yapf

from . import paths, proc


class FormatArguments(Protocol):
    check: bool
    cpp: bool
    py: bool


def start() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--check',
                        help='Check whether files need to be formatted, but do not modify them.',
                        action='store_true')
    parser.add_argument('--no-cpp', help='Skip formatting/checking C++ files', action='store_false', dest='cpp')
    parser.add_argument('--no-py', help='Skip formatting/checking Python files', action='store_false', dest='py')
    args: FormatArguments = parser.parse_args()

    if args.cpp:
        format_cpp(args)
    if args.py:
        format_py(args)


def format_cpp(args: FormatArguments) -> None:
    src_dir = paths.PROJECT_ROOT / 'src'
    cpp_files = src_dir.glob('**/*.[hc]pp')
    cf_args: proc.CommandLine = [
        ('--dry-run', '--Werror') if args.check else (),
        '-i',  # Modify files in-place
        '--verbose',
    ]
    for cf_cand in ('clang-format-10', 'clang-format-9', 'clang-format-8', 'clang-format'):
        cf = paths.find_exe(cf_cand)
        if not cf:
            continue
        break
    else:
        raise RuntimeError('No clang-format executable found')

    print(f'Using clang-format: {cf_cand}')
    res = proc.run([cf, cf_args, cpp_files])
    if res.returncode and args.check:
        raise RuntimeError('Format checks failed for one or more C++ files. (See above.)')
    if res.returncode:
        raise RuntimeError('Format execution failed. Check output above.')


def format_py(args: FormatArguments) -> None:
    py_files = paths.TOOLS_DIR.rglob('*.py')
    rc = yapf.main(
        list(proc.flatten_cmd([
            '--parallel',
            '--verbose',
            ('--diff') if args.check else ('--in-place'),
            py_files,
        ])))
    if rc and args.check:
        raise RuntimeError('Format checks for one or more Python files. (See above.)')
    if rc:
        raise RuntimeError('Format execution failed for Python code. See above.')


if __name__ == "__main__":
    start()
