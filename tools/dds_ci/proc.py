from pathlib import PurePath
from typing import Iterable, Union, Optional, Iterator, NoReturn, Sequence, Mapping
from typing_extensions import Protocol
import subprocess

from .util import Pathish

CommandLineArg = Union[str, Pathish, int, float]
CommandLineArg1 = Union[CommandLineArg, Iterable[CommandLineArg]]
CommandLineArg2 = Union[CommandLineArg1, Iterable[CommandLineArg1]]
CommandLineArg3 = Union[CommandLineArg2, Iterable[CommandLineArg2]]
CommandLineArg4 = Union[CommandLineArg3, Iterable[CommandLineArg3]]


class CommandLine(Protocol):
    def __iter__(self) -> Iterator[Union['CommandLine', CommandLineArg]]:
        pass


# CommandLine = Union[CommandLineArg4, Iterable[CommandLineArg4]]


class ProcessResult(Protocol):
    args: Sequence[str]
    returncode: int
    stdout: bytes
    stderr: bytes


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


def run(*cmd: CommandLine,
        cwd: Optional[Pathish] = None,
        check: bool = False,
        env: Optional[Mapping[str, str]] = None) -> ProcessResult:
    command = list(flatten_cmd(cmd))
    res = subprocess.run(command, cwd=cwd, check=False, env=env)
    if res.returncode and check:
        raise_error(res)
    return res


def raise_error(proc: ProcessResult) -> NoReturn:
    raise subprocess.CalledProcessError(proc.returncode, proc.args, output=proc.stdout, stderr=proc.stderr)


def check_run(*cmd: CommandLine,
              cwd: Optional[Pathish] = None,
              env: Optional[Mapping[str, str]] = None) -> ProcessResult:
    return run(cmd, cwd=cwd, check=True, env=env)
