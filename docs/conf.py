# -*- coding: utf-8 -*-
# Refer: http://www.sphinx-doc.org/en/master/config

import os

# -- Project information -----------------------------------------------------
project = 'dds'
copyright = '2020, vector-of-bool'
author = 'vector-of-bool'

# The short X.Y version
version = ''
# The full version, including alpha/beta/rc tags
release = '0.1.0-alpha.6'

# -- General configuration ---------------------------------------------------
extensions = ['sphinx.ext.autodoc', 'sphinx.ext.intersphinx']
templates_path = []
source_suffix = '.rst'
master_doc = 'index'
language = None
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']
pygments_style = None
intersphinx_mapping = {
    'python': ('https://docs.python.org/3', None),
    'pytest': ('https://docs.pytest.org/en/latest/', None),
}

# -- Options for HTML output -------------------------------------------------
html_theme = 'nature'
html_theme_options = {}
html_static_path = []
html_sidebars = {}

if os.environ.get('GEN_FOR_HUGO'):
    templates_path.append('.')
    html_theme = 'basic'
