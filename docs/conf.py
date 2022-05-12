# -*- coding: utf-8 -*-
# Refer: http://www.sphinx-doc.org/en/master/config

from sphinx.application import Sphinx

from pathlib import Path
from typing import Any
import sys

import hoverxref.extension
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

rst_prolog = r'''
.. include:: /prolog.rst
'''


def setup(app: Sphinx):
    app.add_css_file('styles.css')
