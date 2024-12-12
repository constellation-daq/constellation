# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: EUPL-1.2

"""
Copy Satellite READMEs to documentation and convert them for MyST consumption
"""

import pathlib
import re
import yaml

import sphinx.util.logging

logger = sphinx.util.logging.getLogger(__name__)


def guess_language(directory: pathlib.Path) -> tuple[str, str]:
    """
    Guess the language of the satellite implementation based on the path they have been found in
    """

    if "cxx" in directory.as_posix():
        return "cxx", "C++"
    elif "python" in directory.as_posix():
        return "py", "Python"
    return "", "Unknown"


def find_header_file(lang: str, directory: pathlib.Path) -> tuple[pathlib.Path, str] | None:
    """
    Find the header file of a satellite in the specified directory and the class name.
    For C++, this assumes the 'NameSatellite.hpp' naming scheme.
    For Python, this assumes the `from .XXX import Name` in `__main__` scheme.
    """
    if lang == "cxx":
        for file in directory.glob("*Satellite.hpp"):
            # Return the first matching file (assuming only one matches)
            return file, file.name.removesuffix(".hpp")
    elif lang == "py":
        main_py_file = directory / "__main__.py"
        if main_py_file.exists():
            with open(main_py_file, "r") as file:
                content = file.read()
                # Search for an import statement that imports the desired class  (e.g. `from .Mariner import Mariner`)
                match = re.search(r"from\s+\.(\w+)\s+import\s+(\w+)", content)
                if match:
                    # Extract the module and satellite name from the import statement
                    py_file = directory / f"{match.group(1)}.py"
                    name = match.group(2)
                    if py_file.exists():
                        return py_file, name
    return None


def extract_parent_classes(lang: str, header_path: pathlib.Path, satellite_name: str) -> list[str] | None:
    """
    Extract the parent classes from the C++ header file or Python definition.
    """
    with open(header_path, "r") as file:
        content = file.read()

        if lang == "cxx":
            # Regular expression to find the parent class in C++ inheritance declaration
            match = re.search(rf"class\s+{satellite_name}\s*(?:final)?\s*:\s*public\s+((?:\w+::)*(\w*Satellite))\b", content)
            if match:
                return [match.group(2)]
        elif lang == "py":
            # Regex to match Python class inheritance (might be multiple)
            match = re.search(rf"class\s+{satellite_name}\s*\((.+)\):", content)
            if match:
                # split multiple classes, remove whitespaces
                return match.group(1).replace(" ", "").split(",")
    return None


def convert_satellite_readme(in_path: pathlib.Path, out_path: pathlib.Path) -> tuple[str, str]:
    """
    Converts and copies a satellite README. The output is written to `<out_path>/<satellite_type>.md`.

    Args:
        in_path: Path to README with YAML front-matter.
        out_path: Path to directory where to write the converted <arkdown file.

    Returns:
        Tuple with satellite type (taken from the parent directory) and category name.
    """

    # Guess the language
    lang_code, lang = guess_language(in_path)

    # rewrite the file
    with in_path.open(mode="r", encoding="utf-8") as in_file:
        file_input = in_file.read()
        file_output, category = convert_front_matter(lang, file_input)

        # Parse base class and append configuration parameters
        header_result = find_header_file(lang_code, in_path.parent)
        if header_result:
            header_path, satellite_name = header_result
            logger.verbose(f"Satellite definition file for {satellite_name}: {header_path}")
            # Extract the parent class from the header file
            parent_classes = extract_parent_classes(lang_code, header_path, satellite_name)
            if parent_classes:
                logger.verbose(f"Appending parameters for parent classes: {parent_classes}")
                file_output += "\n```{include} _parameter_header.md\n```\n"
                file_output += append_content(lang_code, parent_classes)
            else:
                logger.warning(f"No parent classes for {satellite_name} found in {header_path}")
        else:
            logger.warning(f"No satellite definition found in {in_path.parent}")

        (out_path / in_path.parent.name).with_suffix(".md").write_text(file_output)

        return in_path.parent.name, category


def append_content(lang: str, parent_classes: list[str]) -> str:
    """
    Append content to the README.md file based on the parent class.
    """
    # List of classes whose content should be appended in order
    if any(class_name in ["ReceiverSatellite", "TransmitterSatellite"] for class_name in parent_classes):
        parent_classes.append("Satellite")

    # Append content for each relevant class
    append = ""
    for class_name in parent_classes:
        content_file = "_" + lang + "_" + class_name + ".md"
        if content_file and (pathlib.Path("satellites") / content_file).exists():
            append += "\n```{include} " + str(content_file) + "\n```\n"
    return append


def convert_front_matter(lang: str, string: str) -> tuple[str, str]:
    """
    Converts YAML front-matter in a string from Markdown to plain text in Markdown.

    Args:
        string: String formatted in Markdown with YAML front-matter.

    Returns:
        Tuple with string formatted in Markdown without YAML front-matter and category name.
    """
    # extract yaml from string
    yaml_match = re.match(r"^---\n(.+?)\n---\n", string, flags=re.DOTALL)
    category = "Uncategorized"

    if yaml_match:
        raw_yaml = yaml_match.group(1)
        yaml_data = yaml.safe_load(raw_yaml)
        yaml_endpos = yaml_match.end(0)
        string_after_yaml = string[yaml_endpos:]

        # get satellite category:
        if "category" in yaml_data.keys():
            category = yaml_data["category"]

        # amend language if missing:
        if lang and "language" not in yaml_data.keys():
            yaml_data["language"] = lang

        # Add title:
        converted_front_matter = "# " + yaml_data["title"] + " Satellite\n"

        # build header table
        converted_front_matter += "| Name | " + yaml_data["title"] + " |\n"
        converted_front_matter += "| ---- | ---- |\n"

        for key, value in yaml_data.items():
            if key == "title":
                continue

            converted_front_matter += "| " + key.capitalize() + " | " + value + " |\n"

        converted_front_matter += "\n"
        converted_front_matter += "## Description\n"

        string = converted_front_matter + string_after_yaml

    return string, category
