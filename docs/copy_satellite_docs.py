# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: EUPL-1.2

"""
Copy Satellite READMEs to documentation and convert them for MyST consumption
"""

import pathlib
import re
import urllib.request

import sphinx.util.logging
import yaml

logger = sphinx.util.logging.getLogger(__name__)


def guess_language(path: pathlib.Path) -> str:
    """
    Guess the language of the satellite implementation based on the path they have been found in
    """
    if "cxx" in path.as_posix():
        return "C++"
    if "python" in path.as_posix():
        return "Python"
    return "Unknown"


def find_satellite_header(language: str, directory: pathlib.Path) -> tuple[pathlib.Path, str] | None:
    """
    Find the header file of a satellite in the specified directory and the class name.
    For C++, this assumes the `NameSatellite.hpp` naming scheme.
    For Python, this assumes the `from .XXX import Name` in `__main__` scheme.
    """
    if language == "C++":
        for file in directory.glob("*Satellite.hpp"):
            # Return the first matching file (assuming only one matches)
            return file, file.name.removesuffix(".hpp")
    elif language == "Python":
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


def extract_parent_classes(language: str, satellite_header: pathlib.Path, satellite_name: str) -> list[str] | None:
    """
    Extract the parent classes from the C++ header file or Python __main__ file
    """
    with open(satellite_header, "r") as file:
        content = file.read()
        if language == "C++":
            # Regular expression to find the parent class in C++ inheritance declaration
            match = re.search(rf"class\s+{satellite_name}\s*(?:final)?\s*:\s*public\s+((?:\w+::)*(\w*Satellite))\b", content)
            if match:
                return [match.group(2)]
        elif language == "Python":
            # Regex to match Python class inheritance (might be multiple)
            match = re.search(rf"class\s+{satellite_name}\s*\((.+)\):", content)
            if match:
                # split multiple classes, remove whitespaces
                return match.group(1).replace(" ", "").split(",")
    return None


def extract_front_matter(markdown: str) -> tuple[str, str, list[str]]:
    """
    Extract category, language and parent classes from a YAML front-matter in Markdown
    """
    category = "Uncategorized"
    language = "Unknown"
    parent_classes = []

    yaml_match = re.match(r"^---\n(.+?)\n---\n", markdown, flags=re.DOTALL)
    if yaml_match:
        raw_yaml = yaml_match.group(1)
        yaml_data = yaml.safe_load(raw_yaml)
        if "category" in yaml_data.keys():
            category = str(yaml_data["category"])
        if "language" in yaml_data.keys():
            language = str(yaml_data["language"])
        if "parent_class" in yaml_data.keys():
            parent_classes = [str(yaml_data["parent_class"])]

    return category, language, parent_classes


def convert_front_matter(markdown: str, extra_front_matter: dict[str, str]):
    """
    Converts YAML front-matter in a string from Markdown to plain text in Markdown
    """
    yaml_match = re.match(r"^---\n(.+?)\n---\n", markdown, flags=re.DOTALL)
    if yaml_match:
        raw_yaml = yaml_match.group(1)
        yaml_data = yaml.safe_load(raw_yaml)
        yaml_endpos = yaml_match.end(0)
        string_after_yaml = markdown[yaml_endpos:]

        # Add title
        converted_front_matter = "# " + yaml_data["title"] + " Satellite\n\n"

        # Build header table
        converted_front_matter += "| Name | " + yaml_data["title"] + " |\n"
        converted_front_matter += "| ---- | ---- |\n"
        skipped_keys = ["title", "parent_class"]
        for key, value in yaml_data.items():
            if key in skipped_keys:
                continue
            converted_front_matter += "| " + key.capitalize() + " | " + value + " |\n"
        for key, value in extra_front_matter.items():
            converted_front_matter += "| " + key.capitalize() + " | " + value + " |\n"

        # Return new Markdown
        return converted_front_matter + string_after_yaml

    # No YAML data found, throw exception
    raise Exception("No YAML data found")


def language_shortcode(language: str) -> str:
    """
    Return the language shortcode used for parent class templates
    """
    if language == "C++":
        return "cxx"
    if language == "Python":
        return "py"
    return language


def append_parent_classes(language: str, parent_classes: list[str]) -> str:
    """
    Append parent classes to the README.md file based on the parent class.
    """
    # List of classes whose content should be appended in order
    if any(class_name in ["ReceiverSatellite", "TransmitterSatellite"] for class_name in parent_classes):
        parent_classes.append("Satellite")

    append = ""

    # Append parameter content for each relevant class
    append += "\n```{include} _parameter_header.md\n```\n"
    for class_name in parent_classes:
        logger.verbose(f"Appending parameters for parent classes: {parent_classes}")
        content_file = "_parameters_" + language_shortcode(language) + "_" + class_name + ".md"
        if content_file and (pathlib.Path("satellites") / content_file).exists():
            append += "\n```{include} " + content_file + "\n```\n"
        else:
            logger.verbose(f"Could not find parent class file {content_file}")

    # Append metric content for each relevant class
    append += "\n```{include} _metric_header.md\n```\n"
    for class_name in parent_classes:
        logger.verbose(f"Appending parameters for parent classes: {parent_classes}")
        content_file = "_metrics_" + language_shortcode(language) + "_" + class_name + ".md"
        if content_file and (pathlib.Path("satellites") / content_file).exists():
            append += "\n```{include} " + content_file + "\n```\n"
        else:
            logger.verbose(f"Could not find parent class file {content_file}")

    return append


def convert_satellite_readme(
    markdown: str, language: str, parent_classes: list[str], extra_front_matter: dict[str, str] = {}
) -> str:
    """
    Convert Markdown front-matter and append parent classes
    """
    # Convert front matter
    markdown = convert_front_matter(markdown, extra_front_matter)
    # Append parent classes
    if parent_classes:
        markdown += append_parent_classes(language, parent_classes)

    return markdown


def convert_satellite_readme_repo(in_path: pathlib.Path, out_path: pathlib.Path) -> tuple[str, str]:
    """
    Converts a satellite README in the repo. The output is written to `<out_path>/<satellite_type>.md`.
    Returns the satellite type and category.
    """
    # Read file contents
    with in_path.open(mode="r", encoding="utf-8") as in_file:
        markdown = in_file.read()

    # Extract category (language and parent classes are not defined in repo READMEs)
    category, language, parent_classes = extract_front_matter(markdown)

    # Extract language
    language = guess_language(in_path)

    # Find parent classes
    header_result = find_satellite_header(language, in_path.parent)
    if header_result:
        satellite_header, satellite_name = header_result
        logger.verbose(f"Satellite definition file for {satellite_name}: {satellite_header}")
        parent_classes = extract_parent_classes(language, satellite_header, satellite_name)
        if parent_classes:
            logger.verbose(f"Appending parent classes: {parent_classes}")
        else:
            logger.warning(f"No parent classes for {satellite_name} found in {satellite_header}")
            parent_classes = []
    else:
        logger.warning(f"No satellite definition found in {in_path.parent}")

    # Convert markdown
    markdown = convert_satellite_readme(markdown, language, parent_classes)

    # Write file
    (out_path / in_path.parent.name).with_suffix(".md").write_text(markdown)

    # Return satellite type and category
    return in_path.parent.name, category


def convert_satellite_readme_ext(name: str, readme_url: str, website: str, out_path: pathlib.Path) -> str | None:
    """
    Converts and copies an external satellite README. The output is written to `<out_path>/<satellite_type>.md`.
    """
    # Run everything in try-except in case no internet connection or other error
    try:
        # Download README
        request = urllib.request.urlopen(readme_url)
        markdown = request.read().decode()

        # Extract category, language and parent classes
        category, language, parent_classes = extract_front_matter(markdown)

        # Convert markdown
        markdown = convert_satellite_readme(markdown, language, parent_classes, {"Website": f"[{website}]({website})"})

        # Write Markdown
        (out_path / name).with_suffix(".md").write_text(markdown)

        # Return category
        return category

    except:  # noqa: E722
        logger.warning(f"Failed to convert external satellite README for {name}")
    return None
