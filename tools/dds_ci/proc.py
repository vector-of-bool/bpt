from pathlib import PurePath, Path
from typing import Iterable, Union
import subprocess

CommandLineArg = Union[str, PurePath, int, float]
CommandLineArg1 = Union[CommandLineArg, Iterable[CommandLineArg]]
CommandLineArg2 = Union[CommandLineArg1, Iterable[CommandLineArg1]]
CommandLineArg3 = Union[CommandLineArg2, Iterable[CommandLineArg2]]
CommandLineArg4 = Union[CommandLineArg3, Iterable[CommandLineArg3]]
CommandLine = Union[CommandLineArg4, Iterable[CommandLineArg4]]


def flatten_cmd(cmd: CommandLine) -> Iterable[str]:
    if isinstance(cmd, (str, PurePath)):
        yield str(cmd)
    elif isinstance(cmd, (int, float)):
        yield str(cmd)
    elif hasattr(cmd, '__iter__'):
        each = (flatten_cmd(arg) for arg in cmd)  # type: ignore
        for item in each:
            yield from item
    else:
        assert False, f'Invalid command line element: {repr(cmd)}'


def run(*cmd: CommandLine, cwd: Path = None) -> subprocess.CompletedProcess:
    return subprocess.run(
        list(flatten_cmd(cmd)),  # type: ignore
        cwd=cwd,
    )


def check_run(*cmd: CommandLine,
              cwd: Path = None) -> subprocess.CompletedProcess:
    flat_cmd = list(flatten_cmd(cmd))  # type: ignore
    res = run(flat_cmd, cwd=cwd)
    if res.returncode != 0:
        raise subprocess.CalledProcessError(res.returncode, flat_cmd)
    return res
