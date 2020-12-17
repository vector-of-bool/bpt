"""
Script for populating a repository with packages declaratively.
"""

import argparse
import itertools
import json
import os
import re
import shutil
import stat
import sys
import tarfile
import tempfile
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
from pathlib import Path
from subprocess import check_call
from threading import Lock
from typing import (Any, Dict, Iterable, Iterator, NamedTuple, NoReturn, Optional, Sequence, Tuple, Type, TypeVar,
                    Union)
from urllib import request

from semver import VersionInfo
from typing_extensions import Protocol

T = TypeVar('T')

I32_MAX = 0xffff_ffff - 1
MAX_VERSION = VersionInfo(I32_MAX, I32_MAX, I32_MAX)

REPO_ROOT = Path(__file__).resolve().absolute().parent.parent


def _get_dds_exe() -> Path:
    suffix = '.exe' if os.name == 'nt' else ''
    dirs = [REPO_ROOT / '_build', REPO_ROOT / '_prebuilt']
    for d in dirs:
        exe = d / ('dds' + suffix)
        if exe.is_file():
            return exe
    raise RuntimeError('Unable to find a dds.exe to use')


class Dependency(NamedTuple):
    name: str
    low: VersionInfo
    high: VersionInfo

    @classmethod
    def parse(cls: Type[T], depstr: str) -> T:
        mat = re.match(r'(.+?)([\^~\+@])(.+?)$', depstr)
        if not mat:
            raise ValueError(f'Invalid dependency string "{depstr}"')
        name, kind, version_str = mat.groups()
        version = VersionInfo.parse(version_str)
        high = {
            '^': version.bump_major,
            '~': version.bump_minor,
            '@': version.bump_patch,
            '+': lambda: MAX_VERSION,
        }[kind]()
        return cls(name, version, high)


def glob_if_exists(path: Path, pat: str) -> Iterable[Path]:
    try:
        yield from path.glob(pat)
    except FileNotFoundError:
        yield from ()


class MoveTransform(NamedTuple):
    frm: str
    to: str
    strip_components: int = 0
    include: Sequence[str] = []
    exclude: Sequence[str] = []

    @classmethod
    def parse_data(cls: Type[T], data: Any) -> T:
        return cls(frm=data.pop('from'),
                   to=data.pop('to'),
                   include=data.pop('include', []),
                   strip_components=data.pop('strip-components', 0),
                   exclude=data.pop('exclude', []))

    def apply_to(self, p: Path) -> None:
        src = p / self.frm
        dest = p / self.to
        if src.is_file():
            self.do_reloc_file(src, dest)
            return

        inc_pats = self.include or ['**/*']
        include = set(itertools.chain.from_iterable(glob_if_exists(src, pat) for pat in inc_pats))
        exclude = set(itertools.chain.from_iterable(glob_if_exists(src, pat) for pat in self.exclude))
        to_reloc = sorted(include - exclude)
        for source_file in to_reloc:
            relpath = source_file.relative_to(src)
            strip_relpath = Path('/'.join(relpath.parts[self.strip_components:]))
            dest_file = dest / strip_relpath
            self.do_reloc_file(source_file, dest_file)

    def do_reloc_file(self, src: Path, dest: Path) -> None:
        if src.is_dir():
            dest.mkdir(exist_ok=True, parents=True)
        else:
            dest.parent.mkdir(exist_ok=True, parents=True)
            src.rename(dest)


class CopyTransform(MoveTransform):
    def do_reloc_file(self, src: Path, dest: Path) -> None:
        if src.is_dir():
            dest.mkdir(exist_ok=True, parents=True)
        else:
            shutil.copy2(src, dest)


class OneEdit(NamedTuple):
    kind: str
    line: int
    content: Optional[str] = None

    @classmethod
    def parse_data(cls, data: Dict) -> 'OneEdit':
        return OneEdit(data.pop('kind'), data.pop('line'), data.pop('content', None))

    def apply_to(self, fpath: Path) -> None:
        fn = {
            'insert': self._insert,
            # 'delete': self._delete,
        }[self.kind]
        fn(fpath)

    def _insert(self, fpath: Path) -> None:
        content = fpath.read_bytes()
        lines = content.split(b'\n')
        assert self.content
        lines.insert(self.line, self.content.encode())
        fpath.write_bytes(b'\n'.join(lines))


class EditTransform(NamedTuple):
    path: str
    edits: Sequence[OneEdit] = []

    @classmethod
    def parse_data(cls, data: Dict) -> 'EditTransform':
        return EditTransform(data.pop('path'), [OneEdit.parse_data(ed) for ed in data.pop('edits')])

    def apply_to(self, p: Path) -> None:
        fpath = p / self.path
        for ed in self.edits:
            ed.apply_to(fpath)


class WriteTransform(NamedTuple):
    path: str
    content: str

    @classmethod
    def parse_data(self, data: Dict) -> 'WriteTransform':
        return WriteTransform(data.pop('path'), data.pop('content'))

    def apply_to(self, p: Path) -> None:
        fpath = p / self.path
        print('Writing to file', p, self.content)
        fpath.write_text(self.content)


class RemoveTransform(NamedTuple):
    path: Path
    only_matching: Sequence[str] = ()

    @classmethod
    def parse_data(self, d: Any) -> 'RemoveTransform':
        p = d.pop('path')
        pat = d.pop('only-matching')
        return RemoveTransform(Path(p), pat)

    def apply_to(self, p: Path) -> None:
        if p.is_dir():
            self._apply_dir(p)
        else:
            p.unlink()

    def _apply_dir(self, p: Path) -> None:
        abspath = p / self.path
        if not self.only_matching:
            # Remove everything
            if abspath.is_dir():
                better_rmtree(abspath)
            else:
                abspath.unlink()
            return

        for pat in self.only_matching:
            items = glob_if_exists(abspath, pat)
            for f in items:
                if f.is_dir():
                    better_rmtree(f)
                else:
                    f.unlink()


class FSTransform(NamedTuple):
    copy: Optional[CopyTransform] = None
    move: Optional[MoveTransform] = None
    remove: Optional[RemoveTransform] = None
    write: Optional[WriteTransform] = None
    edit: Optional[EditTransform] = None

    def apply_to(self, p: Path) -> None:
        for tr in (self.copy, self.move, self.remove, self.write, self.edit):
            if tr:
                tr.apply_to(p)

    @classmethod
    def parse_data(self, data: Any) -> 'FSTransform':
        move = data.pop('move', None)
        copy = data.pop('copy', None)
        remove = data.pop('remove', None)
        write = data.pop('write', None)
        edit = data.pop('edit', None)
        return FSTransform(
            copy=None if copy is None else CopyTransform.parse_data(copy),
            move=None if move is None else MoveTransform.parse_data(move),
            remove=None if remove is None else RemoveTransform.parse_data(remove),
            write=None if write is None else WriteTransform.parse_data(write),
            edit=None if edit is None else EditTransform.parse_data(edit),
        )


class HTTPRemoteSpec(NamedTuple):
    url: str
    transform: Sequence[FSTransform]

    @classmethod
    def parse_data(cls, data: Dict[str, Any]) -> 'HTTPRemoteSpec':
        url = data.pop('url')
        trs = [FSTransform.parse_data(tr) for tr in data.pop('transforms', [])]
        return HTTPRemoteSpec(url, trs)

    def make_local_dir(self):
        return http_dl_unpack(self.url)


class GitSpec(NamedTuple):
    url: str
    ref: str
    transform: Sequence[FSTransform]

    @classmethod
    def parse_data(cls, data: Dict[str, Any]) -> 'GitSpec':
        ref = data.pop('ref')
        url = data.pop('url')
        trs = [FSTransform.parse_data(tr) for tr in data.pop('transform', [])]
        return GitSpec(url=url, ref=ref, transform=trs)

    @contextmanager
    def make_local_dir(self) -> Iterator[Path]:
        tdir = Path(tempfile.mkdtemp())
        try:
            check_call(['git', 'clone', '--quiet', self.url, f'--depth=1', f'--branch={self.ref}', str(tdir)])
            yield tdir
        finally:
            better_rmtree(tdir)


class ForeignPackage(NamedTuple):
    remote: Union[HTTPRemoteSpec, GitSpec]
    transform: Sequence[FSTransform]
    auto_lib: Optional[Tuple]

    @classmethod
    def parse_data(cls, data: Dict[str, Any]) -> 'ForeignPackage':
        git = data.pop('git', None)
        http = data.pop('http', None)
        chosen = git or http
        assert chosen, data
        trs = data.pop('transform', [])
        al = data.pop('auto-lib', None)
        return ForeignPackage(
            remote=GitSpec.parse_data(git) if git else HTTPRemoteSpec.parse_data(http),
            transform=[FSTransform.parse_data(tr) for tr in trs],
            auto_lib=al.split('/') if al else None,
        )

    @contextmanager
    def make_local_dir(self, name: str, ver: VersionInfo) -> Iterator[Path]:
        with self.remote.make_local_dir() as tdir:
            for tr in self.transform:
                tr.apply_to(tdir)
            if self.auto_lib:
                pkg_json = {
                    'name': name,
                    'version': str(ver),
                    'namespace': self.auto_lib[0],
                }
                lib_json = {'name': self.auto_lib[1]}
                tdir.joinpath('package.jsonc').write_text(json.dumps(pkg_json))
                tdir.joinpath('library.jsonc').write_text(json.dumps(lib_json))
            yield tdir


class SpecPackage(NamedTuple):
    name: str
    version: VersionInfo
    depends: Sequence[Dependency]
    description: str
    remote: ForeignPackage

    @classmethod
    def parse_data(cls, name: str, version: str, data: Any) -> 'SpecPackage':
        deps = data.pop('depends', [])
        desc = data.pop('description', '[No description]')
        remote = ForeignPackage.parse_data(data.pop('remote'))
        return SpecPackage(name,
                           VersionInfo.parse(version),
                           description=desc,
                           depends=[Dependency.parse(d) for d in deps],
                           remote=remote)


def iter_spec(path: Path) -> Iterable[SpecPackage]:
    data = json.loads(path.read_text())
    pkgs = data['packages']
    return iter_spec_packages(pkgs)


def iter_spec_packages(data: Dict[str, Any]) -> Iterable[SpecPackage]:
    for name, versions in data.items():
        for version, defin in versions.items():
            yield SpecPackage.parse_data(name, version, defin)


def _on_rm_error_win32(fn, filepath, _exc_info):
    p = Path(filepath)
    p.chmod(stat.S_IWRITE)
    p.unlink()


def better_rmtree(dir: Path) -> None:
    if os.name == 'nt':
        shutil.rmtree(dir, onerror=_on_rm_error_win32)
    else:
        shutil.rmtree(dir)


@contextmanager
def http_dl_unpack(url: str) -> Iterator[Path]:
    req = request.urlopen(url)
    tdir = Path(tempfile.mkdtemp())
    ofile = tdir / '.dl-archive'
    try:
        with ofile.open('wb') as fd:
            fd.write(req.read())
        tf = tarfile.open(ofile)
        tf.extractall(tdir)
        tf.close()
        ofile.unlink()
        subdir = next(iter(Path(tdir).iterdir()))
        yield subdir
    finally:
        better_rmtree(tdir)


@contextmanager
def spec_as_local_tgz(dds_exe: Path, spec: SpecPackage) -> Iterator[Path]:
    with spec.remote.make_local_dir(spec.name, spec.version) as clone_dir:
        out_tgz = clone_dir / 'sdist.tgz'
        check_call([str(dds_exe), 'sdist', 'create', f'--project={clone_dir}', f'--out={out_tgz}'])
        yield out_tgz


class Repository:
    def __init__(self, dds_exe: Path, path: Path) -> None:
        self._path = path
        self._dds_exe = dds_exe
        self._import_lock = Lock()

    @property
    def pkg_dir(self) -> Path:
        return self._path / 'pkg'

    @classmethod
    def create(cls, dds_exe: Path, dirpath: Path, name: str) -> 'Repository':
        check_call([str(dds_exe), 'repoman', 'init', str(dirpath), f'--name={name}'])
        return Repository(dds_exe, dirpath)

    @classmethod
    def open(cls, dds_exe: Path, dirpath: Path) -> 'Repository':
        return Repository(dds_exe, dirpath)

    def import_tgz(self, path: Path) -> None:
        check_call([str(self._dds_exe), 'repoman', 'import', str(self._path), str(path)])

    def remove(self, name: str) -> None:
        check_call([str(self._dds_exe), 'repoman', 'remove', str(self._path), name])

    def spec_import(self, spec: Path) -> None:
        all_specs = iter_spec(spec)
        want_import = (s for s in all_specs if self._shoule_import(s))
        pool = ThreadPoolExecutor(10)
        futs = pool.map(self._get_and_import, want_import)
        for res in futs:
            pass

    def _shoule_import(self, spec: SpecPackage) -> bool:
        expect_file = self.pkg_dir / spec.name / str(spec.version) / 'sdist.tar.gz'
        return not expect_file.is_file()

    def _get_and_import(self, spec: SpecPackage) -> None:
        print(f'Import: {spec.name}@{spec.version}')
        with spec_as_local_tgz(self._dds_exe, spec) as tgz:
            with self._import_lock:
                self.import_tgz(tgz)


class Arguments(Protocol):
    dir: Path
    spec: Path
    dds_exe: Path


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--dds-exe', type=Path, help='Path to the dds executable to use', default=_get_dds_exe())
    parser.add_argument('--dir', '-d', help='Path to a repository to manage', required=True, type=Path)
    parser.add_argument('--spec',
                        metavar='<spec-path>',
                        type=Path,
                        required=True,
                        help='Provide a JSON document specifying how to obtain an import some packages')
    args: Arguments = parser.parse_args(argv)
    repo = Repository.open(args.dds_exe, args.dir)
    repo.spec_import(args.spec)

    return 0


def start() -> NoReturn:
    sys.exit(main(sys.argv[1:]))


if __name__ == "__main__":
    start()
