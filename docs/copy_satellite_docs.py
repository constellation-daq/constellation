# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: EUPL-1.2

"""
Copy Satellite READMEs to documentation and convert them for MyST consumption
"""

import pathlib
import re
import yaml


def convert_satellite_readme(in_path: pathlib.Path, out_path: pathlib.Path) -> str:
    """
    Converts and copies a satellite README. The output is written to `<out_path>/<satellite_type>.md`.

    Args:
        in_path: Path to README with YAML front-matter.
        out_path: Path to directory where to write the converted <arkdown file.

    Returns:
        Satellite type (taken from the parent directory).
    """
    with in_path.open(mode="r", encoding="utf-8") as in_file:
        file_input = in_file.read()
        file_output = convert_front_matter(file_input)
        (out_path / in_path.parent.name).with_suffix(".md").write_text(file_output)
        return in_path.parent.name


def convert_front_matter(string: str) -> str:
    """
    Converts YAML front-matter in a string from Markdown to plain text in Markdown.

    Args:
        string: String formatted in Markdown with YAML front-matter.

    Returns:
        String formatted in Markdown without YAML front-matter.
    """
    # extract yaml from string
    yaml_match = re.match(r"^---\n(.+?)\n---\n", string, flags=re.DOTALL)

    if yaml_match:
        raw_yaml = yaml_match.group(1)
        yaml_data = yaml.safe_load(raw_yaml)
        yaml_endpos = yaml_match.end(0)
        string_after_yaml = string[yaml_endpos:]

        converted_front_matter = "# " + yaml_data["title"] + " Satellite\n"

        if "subtitle" in yaml_data.keys():
            converted_front_matter += "\n" + yaml_data["subtitle"] + "\n"

        string = converted_front_matter + string_after_yaml

    return string
