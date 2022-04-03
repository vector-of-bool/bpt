from __future__ import annotations

from pathlib import Path
import itertools
from typing import Iterable
import re
import sys
import difflib

from . import paths

DOCS_REF_RE = re.compile(r'(?<!define )BPT_DOC_REF\s*\((.*?)\)')
DOCS_ERR_RE = re.compile(r'(?<!define )BPT_ERR_REF\s*\((.*?)\)')


def doc_refs_in_code(code: str) -> Iterable[str]:
    """Find referenced documentation pages in the given C++ code"""
    code = code.replace('\n', ' ')
    for mat in DOCS_ERR_RE.finditer(code):
        inner = mat.group(1)
        ref = eval(inner)  # pylint: disable=eval-used
        assert isinstance(ref, str), code
        yield f'err/{ref}'


def doc_refs_in_file(filepath: Path) -> set[str]:
    """Find referenced documenation pages in the given C++ source file"""
    content = filepath.read_text(encoding='utf-8')
    return set(doc_refs_in_code(content))


def scan_doc_references(src_root: Path) -> dict[Path, set[str]]:
    """Find referenced documentation pages in the given C++ source tree"""
    patterns = ('*.h', '*.hpp', '*.c', '*.cpp')
    source_files = list(itertools.chain.from_iterable(src_root.rglob(pat) for pat in patterns))
    refs_by_file = ((fpath, doc_refs_in_file(fpath)) for fpath in source_files)
    return dict(refs_by_file)


def normalize_docname(fpath: Path, root: Path) -> str:
    """
    Normalize the name of a documentation page relative to a documentation root

    :param fpath: The absolute path of the documentation page file.
    :param root: The absolute path of the documentation root directory.
    """
    return fpath.relative_to(root).with_suffix('').as_posix()


def nearest(given: str, cands: Iterable[str]) -> str | None:
    """
    Find the nearest candidate string to the given string

    :param given: An input string.
    :param cands: A set of candidate strings to match.

    :returns: The string in ``cands`` that is most similar to ``given``, or
        ``None`` otherwise.
    """
    return min(cands, key=lambda c: -difflib.SequenceMatcher(None, given, c).ratio(), default=None)


def audit_docrefs_main():
    """
    Entrypoint of ``dds-audit-docrefs``. Finds documentation pages referred to
    in the source code, searching for spelling errors, and searches for any
    error pages that are not referenced anywhere in the source files.
    """
    docs_by_file = scan_doc_references(paths.PROJECT_ROOT / 'src')
    docs_root = paths.PROJECT_ROOT / 'docs'
    all_docs = {normalize_docname(d, docs_root) for d in docs_root.rglob('*.rst')}
    # Keep track of all referenced error docs
    err_docs = {d for d in all_docs if d.startswith('err/')}
    # (No one refers to the index)
    err_docs.remove('err/index')

    errors = False
    for fpath, docs in docs_by_file.items():
        for doc in docs:
            if doc not in all_docs:
                print(f'Source file [{fpath}] refers to non-existent documentation page "{doc}"')
                cand = nearest(doc, all_docs)
                if cand:
                    print(f'  (Did you mean "{cand}"?)')
            err_docs.discard(doc)

    for doc in err_docs:
        print(f'Unreferenced error-documentation page: {doc}')
        errors = True

    if errors:
        sys.exit(1)
