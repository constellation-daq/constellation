"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import tomllib


class ConfigError(Exception):
    pass


class Configuration:
    """Class to track configuration variables and requests."""

    def __init__(self, config: dict = {}):
        """Initialize configuration variables"""
        if not isinstance(config, dict):
            raise ConfigError
        self._config = config
        self._requested_keys: set[str] = set()

    def has_unused_values(self):
        """Check if any unused configuration keys exist."""
        return not self._requested_keys == set(self._config.keys())

    def get_unused_keys(self):
        """Return all unused configuration keys"""
        return set(self._config.keys()).difference(self._requested_keys)

    def setdefault(self, key: str, default: any = None):
        """
        Return value from requested key in config with default value if specified.
        Mark key as requested in configuration.
        """
        self._requested_keys.add(key)
        return self._config.setdefault(key, default)

    def __getitem__(self, key: str):
        """
        Return value from requested key in config.
        Mark key as requested in configuration.
        """
        self._requested_keys.add(key)
        return self._config[key]

    def get_keys(self):
        """Return list of keys in config."""
        return list(self._config.keys())


def get_config(
    config_path: str,
    category: str,
    host_class: str,
    host_device: str | None = None,
):
    """Get configuration of satellite. Specify category to only get part of config."""
    try:
        with open(config_path, "rb") as f:
            config = tomllib.load(f)
    # TODO: Handle errors FileNotFoundError, TypeError, tomllibDecodeError
    except tomllib.TOMLDecodeError:
        raise
        # TODO: Handle TOMLDecodeError

    ret_config = {}
    # Set system configurations
    for key, value in config[category].items():
        if not isinstance(value, dict):
            ret_config[key] = value

    for key, value in config[category][host_class].items():
        if not isinstance(value, dict):
            ret_config[key] = value

    if host_device:
        for key, value in config[category][host_class][host_device].items():
            if not isinstance(value, dict):
                ret_config[key] = value

    return ret_config
