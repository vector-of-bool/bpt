import json
import sys
from contextlib import contextmanager
from pathlib import Path
from typing import Iterator

import distro
import json5

from . import paths
from .util import Pathish


@contextmanager
def fixup_toolchain(json_file: Pathish) -> Iterator[Path]:
    """
    Augment the toolchain at the given path by adding 'ccache' or -fuse-ld=lld,
    if those tools are available on the system. Yields a new toolchain file
    based on 'json_file'
    """
    json_file = Path(json_file)
    data = json5.loads(json_file.read_text())
    # Check if we can add ccache
    ccache = paths.find_exe('ccache')
    if ccache and data.get('compiler_id') in ('gnu', 'clang'):
        print('Found ccache:', ccache)
        data['compiler_launcher'] = [str(ccache)]
    # Check for lld for use with GCC/Clang
    if paths.find_exe('ld.lld') and data.get('compiler_id') in ('gnu', 'clang'):
        print('Linking with `-fuse-ld=lld`')
        data.setdefault('link_flags', []).append('-fuse-ld=lld')
    # Save the new toolchain data
    with paths.new_tempdir() as tdir:
        new_json = tdir / json_file.name
        new_json.write_text(json.dumps(data))
        yield new_json


def get_default_audit_toolchain() -> Path:
    """
    Get the default toolchain that should be used for dev and test based on the
    host platform.
    """
    if distro.id() == 'alpine':
        # Alpine Linux cannot use the full audit mode, as asan and ubsan do not
        # work with musl
        return paths.TOOLS_DIR / 'gcc-9-test.jsonc'
    if sys.platform == 'win32':
        return paths.TOOLS_DIR / 'msvc-audit.jsonc'
    if sys.platform == 'linux':
        return paths.TOOLS_DIR / 'gcc-9-audit.jsonc'
    if sys.platform == 'darwin':
        return paths.TOOLS_DIR / 'gcc-9-audit-macos.jsonc'
    raise RuntimeError(f'Unable to determine the default toolchain (sys.platform is {sys.platform!r})')


def get_default_test_toolchain() -> Path:
    """
    Get the default toolchain that should be used by tests that need a toolchain
    to use for executing dds.
    """
    if sys.platform == 'win32':
        return paths.TESTS_DIR / 'msvc.tc.jsonc'
    if sys.platform in ('linux', 'darwin'):
        return paths.TESTS_DIR / 'gcc-9.tc.jsonc'
    raise RuntimeError(f'Unable to determine the default toolchain (sys.platform is {sys.platform!r})')


def get_default_toolchain() -> Path:
    """
    Get the default toolchain that should be used to generate the release executable
    based on the host platform.
    """
    if sys.platform == 'win32':
        return paths.TOOLS_DIR / 'msvc-rel.jsonc'
    if sys.platform == 'linux':
        return paths.TOOLS_DIR / 'gcc-9-rel.jsonc'
    if sys.platform == 'darwin':
        return paths.TOOLS_DIR / 'gcc-9-rel-macos.jsonc'
    raise RuntimeError(f'Unable to determine the default toolchain (sys.platform is {sys.platform!r})')
