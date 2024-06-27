# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

import pathlib

import sphinx
import sphinx.util.logging

import copy_satellite_docs
import latex_helpers

from constellation.core import __version__
from constellation.core import __version_code_name__

logger = sphinx.util.logging.getLogger(__name__)

# set directories
docsdir = pathlib.Path(__file__).resolve().parent
repodir = docsdir.parent

# metadata
project = "Constellation"
project_copyright = "2024 DESY and the Constellation authors, CC-BY-4.0"
author = "DESY and the Constellation authors"
doc_author = r"Stephan Lachnit\and Simon Spannagel\and and the Constellation Authors"
version = __version__
release = "v" + version + " " + __version_code_name__

# extensions
extensions = [
    "ablog",
    "myst_parser",
    "breathe",
    "sphinx.ext.autodoc",
    "sphinxcontrib.apidoc",
    "sphinxcontrib.plantuml",
    "sphinxcontrib.spelling",
    "sphinx_design",
    "sphinx_favicon",
    "sphinx_copybutton",
    "sphinx.ext.imgconverter",
]

# general settings
source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

# Any paths that contain templates, relative to this directory.
templates_path = ["_templates"]

# HTML settings
html_theme = "pydata_sphinx_theme"
html_logo = docsdir.joinpath("logo/logo.png").as_posix()
html_static_path = ["_static", "logo"]

if pathlib.Path("news/media").exists():
    html_static_path.append("news/media")

html_context = {
    "gitlab_url": "https://gitlab.desy.de",
    "gitlab_user": "constellation",
    "gitlab_repo": "constellation",
    "gitlab_version": "main",
    "doc_path": "docs",
}

html_theme_options = {
    "logo": {
        "text": project,
    },
    "gitlab_url": "https://gitlab.desy.de/constellation/constellation",
    "github_url": "https://github.com/constellation-daq/constellation",
    "icon_links": [
        {
            "name": "News RSS feed",
            "url": "news/atom.xml",
            "icon": "fa-solid fa-rss",
        },
    ],
    "use_edit_page_button": True,
    "secondary_sidebar_items": {
        "operator_guide/**": ["page-toc", "edit-this-page"],
        "application_development/**": ["page-toc", "edit-this-page"],
        "framework_reference/**": ["page-toc", "edit-this-page"],
        "protocols/**": ["page-toc", "edit-this-page"],
        "news/**": ["page-toc"],
        "satellites/**": ["page-toc"],
    },
    "show_prev_next": False,
}

html_css_files = [
    "css/custom.css",
]

html_show_sourcelink = False

html_sidebars = {
    # Blog sidebars (https://ablog.readthedocs.io/en/stable/manual/ablog-configuration-options.html#blog-sidebars)
    "news": ["ablog/categories.html", "ablog/archives.html"],
    "news/**": [
        "ablog/postcard.html",
        "recentposts.html",
        "ablog/categories.html",
        "ablog/archives.html",
    ],
}

# Favicon
favicons = [
    "logo.svg",
]

# LaTeX settings:
latex_engine = "lualatex"
latex_use_xindy = False
latex_elements = {
    "papersize": "a4paper",
    "pointsize": "11pt",
    "figure_align": "tbp",
    "fncychap": "",
    "babel": "",
    "preamble": latex_helpers.preamble,
    "maketitle": latex_helpers.maketitle,
    "classoptions": "captions=tableheading,a4paper,11pt,numbers=noenddot,titlepage,twoside,openright,DIV=14,BCOR=8mm",
}
latex_logo = docsdir.joinpath("logo/logo_small.png").as_posix()
latex_show_urls = "footnote"
latex_theme = "manual"
latex_documents = [
    ("operator_guide/index", "operator_guide.tex", None, doc_author, "manual", False),
    ("application_development/index", "application_development_guide.tex", None, doc_author, "manual", False),
    ("framework_reference/index", "framework_development_guide.tex", None, doc_author, "manual", False),
]

# myst settings
myst_heading_anchors = 4
myst_fence_as_directive = ["plantuml"]
myst_enable_extensions = ["colon_fence"]
myst_update_mathjax = False

# Suppress header warnings from MyST - we check them with markdownlint but cannot disable them in MyST on a per-file level
suppress_warnings = ["myst.header"]

# breathe settings
breathe_projects = {
    "Constellation": docsdir.joinpath("doxygen").joinpath("xml"),
}
breathe_default_project = "Constellation"

# Spell-checking settings
spelling_lang = "en_US"
tokenizer_lang = "en_US"
spelling_word_list_filename = [".spelling_jargon.txt", ".spelling_names.txt"]
spelling_warning = True
spelling_show_suggestions = True

# PlantUML settings
plantuml_output_format = "svg_img"

# Remove news from toc if news/index.md does not exist
without_news = not docsdir.joinpath("news").exists()
if without_news:
    logger.info("Building documentation without news section", color="yellow")

# Remove existing satellite READMEs
for path in (docsdir / "satellites").glob("*.md"):
    if not path.name.startswith("_"):
        path.unlink()

# Add satellite READMEs to documentation
satellite_files = []
satellite_files.extend(list((repodir / "cxx" / "satellites").glob("**/README.md")))
satellite_files.extend(list((repodir / "python" / "constellation" / "satellites").glob("**/README.md")))

# Retrieve satellite type and category
satellites = {}
for path in satellite_files:
    satellite_type, satellite_category = copy_satellite_docs.convert_satellite_readme(path, docsdir / "satellites")
    satellites.setdefault(satellite_category, []).append(f"{satellite_type} <{satellite_type}>")

# Create tocs for categories
satellite_tocs = ""
for category, satellites_list in sorted(satellites.items()):
    satellite_tocs += f"\n```{{toctree}}\n:caption: {category}\n:maxdepth: 1\n\n"
    for satellite in satellites_list:
        satellite_tocs += satellite + "\n"
    satellite_tocs += "```\n"

# Add tocs to satellites index.md.in
with (
    open("satellites/index.md.in", "rt") as index_in,
    open("satellites/index.md", "wt") as index_out,
):
    for line in index_in:
        line = line.replace("SATELLITES", satellite_tocs)
        index_out.write(line)

# ablog settings
blog_title = project
blog_path = "news"
blog_post_pattern = ["news/*.md", "news/*.rst"]
post_date_format = "%Y-%m-%d"
blog_feed_fulltext = True

# apidoc settings
apidoc_module_dir = (repodir / "python" / "constellation").as_posix()
apidoc_excluded_paths = [
    (repodir / "python" / "constellation" / "satellites").as_posix()
]
apidoc_output_dir = (docsdir / "reference" / "python").as_posix()
apidoc_module_first = True
apidoc_separate_modules = True
apidoc_toc_file = False
apidoc_extra_args = ["--implicit-namespaces"]
