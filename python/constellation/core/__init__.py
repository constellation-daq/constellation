"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

# Try to get version from installation
try:
    import importlib.metadata

    __version__ = importlib.metadata.version("constellation")  # module name
except:  # noqa: E722
    __version__ = "0+unknown"
