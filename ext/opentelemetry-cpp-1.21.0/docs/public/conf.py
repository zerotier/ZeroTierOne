# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

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
# import os
# import sys
# sys.path.insert(0, os.path.abspath('.'))

# -- Project information -----------------------------------------------------

project = 'OpenTelemetry C++'
copyright = '2021, OpenTelemetry authors'
author = 'OpenTelemetry authors'

# The full version, including alpha/beta/rc tags
release = "1.21.0"

# Run sphinx on subprojects and copy output
# -----------------------------------------
# This is necessary so the readthedocs build works. It doesn't invoke the
# Makefile, but just runs sphinx on this conf.py.
import os
import shutil
import subprocess
if not os.path.exists('doxyoutput'):
        os.makedirs('doxyoutput')

# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    "breathe",
    "exhale"
]

exhale_args = {
        "containmentFolder": "otel_docs",
        "rootFileName": "otel_docs.rst",
        "rootFileTitle": "Reference documentation",
        "doxygenStripFromPath": "..",
        "exhaleExecutesDoxygen": True,
        "exhaleUseDoxyfile": True,
        "createTreeView": True
}

breathe_projects = {
        "OpenTelemetry C++": "doxyoutput/xml",
}
breathe_default_project = "OpenTelemetry C++"


primary_domain = "cpp"

higlight_language = "cpp"


# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
#html_theme = "furo"
html_theme = "sphinx_rtd_theme"

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']
