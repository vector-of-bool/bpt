from typing import Iterable, Optional, NoReturn, Sequence, Mapping
from typing_extensions import Protocol
import subprocess

import dagon.proc
from dagon.proc import CommandLine

from .util import Pathish


class ProcessResult(Protocol):
    args: Sequence[str]
    returncode: int
    stdout: bytes
    stderr: bytes


def flatten_cmd(cmd: CommandLine) -> Iterable[str]:
    return (str(s) for s in dagon.proc.flatten_cmdline(cmd))


def run(*cmd: CommandLine,
        cwd: Optional[Pathish] = None,
        check: bool = False,
        env: Optional[Mapping[str, str]] = None,
        timeout: Optional[float] = None) -> ProcessResult:
    timeout = timeout or 60 * 5
    command = list(flatten_cmd(cmd))
    res = subprocess.run(command, cwd=cwd, check=False, env=env, timeout=timeout)
    if res.returncode and check:
        raise_error(res)
    return res


def raise_error(proc: ProcessResult) -> NoReturn:
    raise subprocess.CalledProcessError(proc.returncode, proc.args, output=proc.stdout, stderr=proc.stderr)


def check_run(*cmd: CommandLine,
              cwd: Optional[Pathish] = None,
              env: Optional[Mapping[str, str]] = None,
              timeout: Optional[float] = None) -> ProcessResult:
    return run(cmd, cwd=cwd, check=True, env=env, timeout=timeout)
