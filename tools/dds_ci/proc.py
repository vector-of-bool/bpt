from pathlib import PurePath
from typing import Iterable, Union, Optional, Iterator
from typing_extensions import Protocol
import subprocess

from .util import Pathish

CommandLineArg = Union[str, PurePath, int, float]
CommandLineArg1 = Union[CommandLineArg, Iterable[CommandLineArg]]
CommandLineArg2 = Union[CommandLineArg1, Iterable[CommandLineArg1]]
CommandLineArg3 = Union[CommandLineArg2, Iterable[CommandLineArg2]]
CommandLineArg4 = Union[CommandLineArg3, Iterable[CommandLineArg3]]


class CommandLine(Protocol):
    def __iter__(self) -> Iterator[Union['CommandLine', CommandLineArg]]:
        pass


# CommandLine = Union[CommandLineArg4, Iterable[CommandLineArg4]]


class ProcessResult(Protocol):
    returncode: int
    stdout: bytes


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


def run(*cmd: CommandLine, cwd: Optional[Pathish] = None, check: bool = False) -> ProcessResult:
    return subprocess.run(
        list(flatten_cmd(cmd)),
        cwd=cwd,
        check=check,
    )


def check_run(*cmd: CommandLine, cwd: Optional[Pathish] = None) -> ProcessResult:
    return subprocess.run(
        list(flatten_cmd(cmd)),
        cwd=cwd,
        check=True,
    )
