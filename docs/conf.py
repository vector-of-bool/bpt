# -*- coding: utf-8 -*-
# Refer: http://www.sphinx-doc.org/en/master/config
from __future__ import annotations

from sphinx.application import Sphinx

from pathlib import Path
from typing import Any, ClassVar, Iterator, Literal, NamedTuple, NewType, Sequence, TypedDict, cast, Iterable
import sys

import re
import hoverxref.extension
from sphinx.domains import Domain, ObjType
from sphinx.directives import ObjectDescription
from docutils.parsers.rst.directives import flag
from sphinx.roles import XRefRole
from sphinx.util.docutils import SphinxRole
from sphinx import addnodes
from sphinx.util.docfields import Field, GroupedField
from sphinx.util.nodes import make_refnode
from docutils import nodes
from docutils.nodes import Element, emphasis
from sphinx.util.typing import OptionSpec
from sphinx.environment import BuildEnvironment
from sphinx.builders import Builder
from sphinx.util.fileutil import copy_asset

# -- Project information -----------------------------------------------------
project = 'bpt'
copyright = '2022'
author = 'vector-of-bool'

# The short X.Y version
version = ''
# The full version, including alpha/beta/rc tags
release = '0.1.0-alpha.6'

# -- General configuration ---------------------------------------------------
extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.intersphinx',
    'hoverxref.extension',
]
templates_path = []
source_suffix = '.rst'
master_doc = 'index'
language = None
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store', 'prolog.rst']
pygments_style = None
intersphinx_mapping = {
    'python': ('https://docs.python.org/3', None),
    'pytest': ('https://docs.pytest.org/en/latest/', None),
}
nitpicky = True
add_function_parenthesis = False

if sys.version_info < (3, 10):
    # Sphinx in older Python fails to handle many typing annotations
    autodoc_warningiserror = False
    autodoc_typehints = 'none'

# -- Options for HTML output -------------------------------------------------
html_theme = 'pyramid'
html_theme_options = {}
html_static_path = ['static']
html_sidebars = {}

hoverxref_role_types = {
    'term': 'tooltip',
    'ref': 'tooltip',
    'envvar': 'tooltip',
    'option': 'tooltip',
    'schematic:prop': 'tooltip',
    'prop': 'tooltip',
}

hoverxref_roles = list(hoverxref_role_types.keys())
hoverxref_auto_ref = True
hoverxref_mathjax = True


def intercept_copy_asset(path: str, dest: str, context: Any) -> None:
    p = Path(path)
    if p.name == 'hoverxref.js_t':
        print(f'Intercept: {p}')
        copy_asset(str(Path(__file__).resolve().parent.joinpath('hoverxref.js_t')), dest, context=context)
    else:
        copy_asset(path, dest, context=context)


hoverxref.extension.copy_asset = intercept_copy_asset

_PARENT_KEY = 'schematic:DOTPATH'

_FullNameStr = NewType('_FullName', str)
_Ident = NewType("_Ident", str)


def _fullname(env: BuildEnvironment, name: str) -> _FullNameStr:
    if '.' in name:
        return _FullNameStr(name)

    par: Sequence[str] = env.ref_context.get(_PARENT_KEY, [])
    if par:
        return _FullNameStr('.'.join(par) + '.' + name)
    return _FullNameStr(name)


def _shortname(fn: _FullNameStr) -> str:
    d = fn.split('.')
    return d[-1]


def _makeid(env: BuildEnvironment, objtype: str, name: _FullNameStr) -> _Ident:
    return _Ident(f'sch-{objtype}-{name}')


def _dotpath(env: BuildEnvironment) -> Sequence[str]:
    return tuple(env.ref_context.setdefault(_PARENT_KEY, []))


class _SchematicEntry(TypedDict):
    parent: Sequence[str]
    shortname: str
    docname: str
    node_id: _Ident
    objtype: Literal['property', 'mapping']


class _SchematicInventory(TypedDict):
    objects: dict[str, _SchematicEntry]


class SchematicObject(ObjectDescription[tuple[_FullNameStr, _Ident]]):

    has_content = True
    required_arguments = 1
    optional_arguments = 0
    final_argument_whitespace = False
    option_spec: OptionSpec = {}
    obj_type_label: ClassVar[str]

    def add_target_and_index(self, name: tuple[_FullNameStr, _Ident], sig: str, signode: addnodes.desc_signature):
        # assert 0, f'Updating inventory {ident=} {self.state.document=}'
        fullname, ident = name
        shortname = _shortname(fullname)
        if ident not in self.state.document.ids:
            signode['names'].append(ident)
            signode['ids'].append(ident)
            self.state.document.note_explicit_target(signode)

        dom = cast(SchematicDomain, self.env.get_domain('schematic'))
        assert self.objtype in ('property', 'mapping')
        dom.data['objects'][fullname] = {
            'parent': _dotpath(self.env),
            'shortname': _shortname(fullname),
            'docname': self.env.docname,
            'node_id': ident,
            'objtype': self.objtype,
        }
        # dom.objects
        entry = ('pair', f'{self.objtype} ; {shortname}', ident, 'main', None)
        self.indexnode['entries'].append(entry)  # type: ignore

    def handle_signature(self, sig: str, signode: addnodes.desc_signature) -> tuple[_FullNameStr, _Ident]:
        signode += addnodes.desc_annotation('', '', *self.signature_prefix(), addnodes.desc_sig_space())
        signode += addnodes.desc_name(sig, sig)
        suffix = tuple(self.signature_suffix())
        if suffix:
            signode += addnodes.desc_sig_space()
            signode += suffix
        fn = _fullname(self.env, sig)
        id = _makeid(self.env, self.objtype, fn)
        return fn, id

    def signature_prefix(self) -> 'Iterable[Element]':
        return [addnodes.desc_sig_keyword(self.objtype, self.objtype)]

    def signature_suffix(self) -> 'Iterable[Element]':
        return []

    def before_content(self):
        name: str = self.arguments[0].strip()
        self.env.ref_context.setdefault(_PARENT_KEY, []).append(name)

    def after_content(self) -> None:
        self.env.ref_context[_PARENT_KEY].pop()
        return super().after_content()


class SchematicMapping(SchematicObject):
    pass


class _PropertyBaseType(NamedTuple):
    spelling: str

    def nodes(self, env: BuildEnvironment) -> Iterator[nodes.Node]:
        if self.spelling in ('undefined', 'number', 'integer', 'null', 'string', 'boolean', 'unknown'):
            yield nodes.Text(self.spelling)
        else:
            ref = addnodes.pending_xref('',
                                        nodes.Text(self.spelling),
                                        reftarget=self.spelling,
                                        reftype='mapping',
                                        refdomain='schematic')
            ref[_PARENT_KEY] = _dotpath(env)
            yield ref


class _PropertyDottedType(NamedTuple):
    parent: _PropertyBaseType | _PropertyDottedType
    part: str

    def nodes(self, env: BuildEnvironment) -> Iterator[nodes.Node]:
        yield from self.parent.nodes(env)
        yield nodes.Text(self.part)


class _PropertyArrayType(NamedTuple):
    type: _PropertyUnionType | _PropertyBaseType | _PropertyDottedType | _PropertyArrayType

    def nodes(self, env: BuildEnvironment) -> Iterator[nodes.Node]:
        yield from self.type.nodes(env)
        yield nodes.Text('[]')


class _PropertyUnionType(NamedTuple):
    alternatives: Sequence[_PropertyBaseType | _PropertyArrayType | _PropertyDottedType]

    def nodes(self, env: BuildEnvironment) -> Iterator[nodes.Node]:
        yield nodes.Text('(')
        it = iter(self.alternatives)
        cur = next(it)
        while cur is not None:
            yield from cur.nodes(env)
            cur = next(it, None)
            if cur:
                yield nodes.Text(' | ')
        yield nodes.Text(')')


def _parse_dotted_type(spell: str) -> tuple[_PropertyDottedType | _PropertyBaseType, str]:
    spell = spell.strip()
    mat = re.match(r'([\w\$]+)(.*)', spell)
    if not mat:
        raise ValueError(f'Invalid type string "{spell}"')
    base, tail = mat.groups()
    typ = _PropertyBaseType(base)
    while mat := re.match(r'\.([\w\$]+)(.*)', tail):
        n, tail = mat.groups()
        typ = _PropertyDottedType(typ, n)
    return typ, tail


def _parse_union_alt(
        spell: str) -> tuple[_PropertyBaseType | _PropertyDottedType | _PropertyArrayType | _PropertyUnionType, str]:
    spell = spell.strip()
    if spell.startswith('('):
        inner = spell[1:]
        typ, tail = _parse_type_str(inner)
        if not tail.startswith(')'):
            raise ValueError(f'Unclosed parenthesis in type string "{spell}"')
        tail = tail[1:].strip()
    else:
        typ, tail = _parse_dotted_type(spell)
    while tail.startswith('[]'):
        typ = _PropertyArrayType(typ)
        tail = tail[2:].strip()
    return typ, tail


def _parse_type_str(
        spell: str) -> tuple[_PropertyBaseType | _PropertyUnionType | _PropertyArrayType | _PropertyDottedType, str]:
    typ, tail = _parse_union_alt(spell)
    tail = tail.strip()
    if not tail:
        return typ, tail

    alts: list[_PropertyBaseType | _PropertyArrayType | _PropertyDottedType] = []
    if isinstance(typ, _PropertyUnionType):
        alts.extend(typ.alternatives)
    else:
        alts.append(typ)
    while tail.startswith('|'):
        tail = tail[1:].strip()
        n, tail = _parse_union_alt(tail)
        if isinstance(n, _PropertyUnionType):
            alts.extend(n.alternatives)
        else:
            alts.append(n)
        tail = tail.strip()
    return _PropertyUnionType(tuple(alts)), tail


def parse_type_str(spell: str) -> _PropertyBaseType | _PropertyUnionType | _PropertyArrayType | _PropertyDottedType:
    typ, tail = _parse_type_str(spell)
    if tail:
        raise ValueError(f'Invalid trailing content "{tail}" in type string "{spell}"')
    return typ


class SchematicProperty(SchematicObject):
    option_spec: OptionSpec = {
        'required': flag,
        'optional': flag,
    }

    doc_field_types = [
        Field('type', ('type', ), label='Type', has_arg=False, bodyrolename='type'),
        GroupedField('option', ('option', ), label='Options', rolename='literal'),
        GroupedField('placeholder', ('placeholder', ), label='Placeholders', rolename='literal'),
    ]

    # optional_arguments = 500

    def signature_suffix(self):
        r: list[Element] = []
        if 'required' in self.options:
            r.append(emphasis('', '(required)'))
        if 'optional' in self.options:
            r.append(emphasis('', '(optional)'))
        return r


class SchematicXRefRole(XRefRole):

    def process_link(self, env: "BuildEnvironment", refnode: Element, has_explicit_title: bool, title: str,
                     target: str) -> tuple[str, str]:
        refnode[_PARENT_KEY] = _dotpath(env)
        if not has_explicit_title:
            if title.startswith('~'):
                target = title[1:]
                last_dot = title.rfind('.')
                if last_dot >= 0:
                    title = title[last_dot + 1:]
        return super().process_link(env, refnode, has_explicit_title, title, target)


class SchematicTypeRole(SphinxRole):

    def run(self) -> tuple[list[nodes.Node], list[nodes.system_message]]:
        t = parse_type_str(self.text)
        return [nodes.literal('', '', *(t.nodes(self.env)))], []


class _LiteralRole(SphinxRole):

    def run(self) -> tuple[list[nodes.Node], list[nodes.system_message]]:
        return [nodes.literal('', self.text)], []


class SchematicDomain(Domain):
    """Domain for JSON/YAML/config schema"""
    name = 'schematic'
    label = 'Schematic'
    object_types = {
        'mapping': ObjType('mapping', 'mapping'),
        'property': ObjType('property', 'prop'),
    }
    directives = {
        'mapping': SchematicMapping,
        'property': SchematicProperty,
    }
    roles = {
        'mapping': SchematicXRefRole(),
        'prop': SchematicXRefRole(),
        'type': SchematicTypeRole(),
        'literal': _LiteralRole(),
    }
    initial_data = {
        'objects': {},
    }

    def __init__(self, *args: Any, **kwargs: Any) -> None:
        self.data: _SchematicInventory
        super().__init__(*args, **kwargs)

    def merge_domaindata(self, docnames: list[str], otherdata: _SchematicInventory) -> None:
        for k, e in otherdata['objects'].items():
            self.data['objects'][k] = e

    def find_obj(self, node: addnodes.pending_xref, name: str) -> _SchematicEntry | None:
        if '.' in name:
            return self.data['objects'].get(name)

        paths: Sequence[str] = node.get(_PARENT_KEY, [])
        if paths:
            partials = reversed(['.'.join(paths[:i]) for i in range(len(paths))])
        else:
            partials = [name]
        for cand in partials:
            found = self.data['objects'].get(cand)
            if found is not None:
                return found

        return self.data['objects'].get(name)

    def resolve_xref(self, env: BuildEnvironment, fromdocname: str, builder: Builder, typ: Literal['mapping', 'prop'],
                     target: str, node: addnodes.pending_xref, contnode: Element):
        ent: _SchematicEntry | None
        ent = self.find_obj(node, target)
        if ent is None:
            raise RuntimeError(f'No such {typ} "{target}"')
        # assert , f'{ident=} {self.data=}'
        return make_refnode(builder, fromdocname, ent['docname'], ent['node_id'], contnode, target)


rst_prolog = r'''
.. include:: /prolog.rst
'''


def setup(app: Sphinx):
    app.add_css_file('styles.css')
    app.add_domain(SchematicDomain)
