import argparse
import json
import os
from pathlib import Path
from typing import Optional, Dict, Any
from typing_extensions import Protocol

from . import paths


class Arguments(Protocol):
    out: Optional[Path]


def gen_task_json_data() -> Dict[str, Any]:
    dds_ci_exe = paths.find_exe('dds-ci')
    assert dds_ci_exe, 'Unable to find the dds-ci executable. This command should be run in a Poetry'
    envs = {key: os.environ[key]
            for key in (
                'CL',
                '_CL_',
                'PATH',
                'INCLUDE',
                'LIBPATH',
                'LIB',
            ) if key in os.environ}
    task = {
        'label': 'MSVC Build',
        'type': 'process',
        'command': str(dds_ci_exe.resolve()),
        'args': ['--rapid'],
        'group': {
            'kind': 'build',
        },
        'options': {
            'env': envs,
        },
        'problemMatcher': '$msCompile',
    }
    return task


def generate_vsc_task() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', '-o', help='File to write into', type=Path)
    args: Arguments = parser.parse_args()

    cl = paths.find_exe('cl')
    if cl is None:
        raise RuntimeError('There is not cl.exe on your PATH. You need to run '
                           'this command from within a Visual Studio environment.')

    data = gen_task_json_data()
    task_str = json.dumps(data, indent=4)
    if args.out:
        args.out.write_text(task_str)
        print(f'The task JSON has been written to {args.out}.')
    else:
        print(task_str)
        print('^^^ The task JSON has been written above ^^^')
    print('Add the JSON object to "tasks.json" to use it in VS Code')
