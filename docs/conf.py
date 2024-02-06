# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
import os
import sys
import subprocess
import itertools
import functools
import operator
sys.path.insert(0, os.path.abspath('..'))


# -- Project information -----------------------------------------------------

project = 'ChampSim'
copyright = '2023, The ChampSim Contributors'
author = 'The ChampSim Contributors'


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    'sphinx.ext.githubpages',
    'sphinx.ext.autodoc',
    'breathe'
]

# The root document
root_doc = 'index'

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

# -- Breathe configuration ---------------------------------------------------
breathe_projects = {
    project: "./_doxygen/xml"
}
breathe_default_project = project

# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'nature'

def get_cmd_lines(cmd):
    return subprocess.run(cmd, capture_output=True).stdout.decode().splitlines()

@functools.cache
def get_current_branch():
    return get_cmd_lines(['git', 'rev-parse', '--abbrev-ref', 'HEAD'])[0]

def get_branches():
    result = [ l[2:] for l in get_cmd_lines(['git', 'branch', '--list', '--no-color'])] # remove '* ' marker for current branch
    return [
        'master',
        'develop',
        *(b for b in result if b.startswith('release/')),
        *(b for b in result if b.startswith('feature/'))
    ]

def get_files(branch=None):
    return get_cmd_lines(['git', 'ls-tree', '-r', '--name-only', branch or get_current_branch(), '--', 'src/'])

def file_branch_map():
    branch_to_file = { b: get_files(b) for b in get_branches() }

    file_branch_pairs = list(itertools.chain(*(zip(f, itertools.repeat(b)) for b,f in branch_to_file.items())))
    file_branch_pairs = sorted(file_branch_pairs, key=operator.itemgetter(0))
    file_to_branch = { os.path.splitext(f[4:])[0]: [b[1] for b in branchlist] for f,branchlist in itertools.groupby(file_branch_pairs, key=operator.itemgetter(0)) }

    return file_to_branch

html_context = {
    "current_version": get_current_branch(),
    "up_prefix": { b: '/'.join(['..']*len(get_current_branch().split('/')) + [b]) for b in get_branches() },
    "branches": get_branches(),
    "versions": file_branch_map()
}

html_sidebars = {
    '**': ['localtoc.html', 'relations.html', 'sourcelink.html', 'other_branches.html', 'searchbox.html']
}

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
#html_static_path = ['_static']

