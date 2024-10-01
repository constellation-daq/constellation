# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

import sphinx
import pathlib

import copy_satellite_docs

logger = sphinx.util.logging.getLogger(__name__)

# set directories
docsdir = pathlib.Path(__file__).resolve().parent
repodir = docsdir.parent

# metadata
project = "Constellation"
project_copyright = "2024 DESY and the Constellation authors, CC-BY-4.0"
author = "DESY and the Constellation authors"
version = "0"
release = "v" + version

# extensions
extensions = [
    "ablog",
    "pydata_sphinx_theme",
    "myst_parser",
    "breathe",
    "sphinxcontrib.plantuml",
    "sphinx_design",
    "sphinx_favicon",
    "sphinx_copybutton",
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
        "manual/**": ["page-toc", "edit-this-page"],
        "reference/**": ["page-toc", "edit-this-page"],
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

# myst settings
myst_heading_anchors = 4
myst_fence_as_directive = ["plantuml"]
myst_enable_extensions = ["colon_fence"]
myst_update_mathjax = False

# breathe settings
breathe_projects = {
    "Constellation": docsdir.joinpath("doxygen").joinpath("xml"),
}
breathe_default_project = "Constellation"

# PlantUML settings
plantuml_output_format = "svg_img"

# Remove news from toc if news/index.md does not exist
without_news = not docsdir.joinpath("news").exists()
if without_news:
    logger.info("Building documentation without news section", color="yellow")
with open("index.md.in", "rt") as index_in, open("index.md", "wt") as index_out:
    for line in index_in:
        if without_news and "news/index" in line:
            continue
        index_out.write(line)

# Remove existing satellite READMEs
for path in (docsdir / "satellites").glob("*.md"):
    path.unlink()

# Add satellite READMEs to documentation
satellite_files_cxx = sorted(
    list((repodir / "cxx" / "satellites").glob("**/README.md"))
)
satellite_files_py = sorted(
    list((repodir / "python" / "constellation" / "satellites").glob("**/README.md"))
)

satellites_types_cxx = []
satellites_types_py = []

for path in satellite_files_cxx:
    satellite_type = copy_satellite_docs.convert_satellite_readme(
        path, docsdir / "satellites"
    )
    satellites_types_cxx.append(f"{satellite_type} <{satellite_type}>")

for path in satellite_files_py:
    satellite_type = copy_satellite_docs.convert_satellite_readme(
        path, docsdir / "satellites"
    )
    satellites_types_py.append(f"{satellite_type} <{satellite_type}>")

with (
    open("satellites/index.md.in", "rt") as index_in,
    open("satellites/index.md", "wt") as index_out,
):
    for line in index_in:
        line = line.replace("SATELLITES_CXX", "\n".join(satellites_types_cxx))
        line = line.replace("SATELLITES_PYTHON", "\n".join(satellites_types_py))
        index_out.write(line)

# ablog settings
blog_title = project
blog_path = "news"
blog_post_pattern = ["news/*.md", "news/*.rst"]
post_date_format = "%Y-%m-%d"
blog_feed_fulltext = True
